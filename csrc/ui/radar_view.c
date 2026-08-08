#define _POSIX_C_SOURCE 200809L

#include "ansiradar80/app.h"
#include "ansiradar80/ansi.h"
#include "ansiradar80/input.h"
#include "ansiradar80/radar.h"
#include "ansiradar80/ui.h"

#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

static void utc_now(char *output, size_t size) {
    time_t now = time(NULL);
    struct tm *value = gmtime(&now);
    if (value == NULL) {
        snprintf(output, size, "--:--:--");
        return;
    }
    strftime(output, size, "%H:%M:%S", value);
}

static void debug_event(const AppConfig *config, const char *event);

static double monotonic_seconds(void) {
    struct timespec value;
    if (clock_gettime(CLOCK_MONOTONIC, &value) != 0) return 0.0;
    return (double)value.tv_sec + (double)value.tv_nsec / 1000000000.0;
}

static void overlay_box(Screen *screen, int x, int y, int width, int height) {
    int ix;
    int iy;
    for (ix = 0; ix < width; ++ix) {
        screen_put(screen, x + ix, y, ix == 0 ? '+' : (ix == width - 1 ? '+' : '-'), 15);
        screen_put(screen, x + ix, y + height - 1, ix == 0 ? '+' : (ix == width - 1 ? '+' : '-'), 15);
    }
    for (iy = 1; iy < height - 1; ++iy) {
        screen_put(screen, x, y + iy, '|', 15);
        screen_put(screen, x + width - 1, y + iy, '|', 15);
    }
}

void ui_draw_help(Screen *screen) {
    overlay_box(screen, 10, 4, 60, 15);
    screen_text(screen, 25, 5, "ANSIRadar-C controls", 15);
    screen_text(screen, 14, 7, "Arrows select   +/- zoom   Tab sort", 7);
    screen_text(screen, 14, 8, "Space center   Enter details   L list", 7);
    screen_text(screen, 14, 9, "H help         Esc closes popup", 7);
    screen_text(screen, 14, 10, "Q quit", 7);
}

void ui_draw_details(Screen *screen, const AircraftList *list, const RadarState *state) {
    size_t order[ANSIRADAR80_MAX_AIRCRAFT];
    const Aircraft *aircraft;
    char line[64];
    size_t count = radar_order(list, state, order, ANSIRADAR80_MAX_AIRCRAFT);
    double distance;
    double bearing;
    if (count == 0 || state->selected >= count) return;
    aircraft = &list->items[order[state->selected]];
    distance = radar_distance_nm(state->receiver_latitude, state->receiver_longitude,
                                 aircraft->latitude, aircraft->longitude);
    bearing = radar_bearing_deg(state->receiver_latitude, state->receiver_longitude,
                                aircraft->latitude, aircraft->longitude);
    overlay_box(screen, 25, 3, 32, 16);
    screen_text(screen, 29, 5, aircraft->callsign[0] ? aircraft->callsign : aircraft->icao, 15);
    screen_text(screen, 28, 6, "ICAO:", 7);
    screen_text(screen, 34, 6, aircraft->icao, 7);
    snprintf(line, sizeof(line), "ALT: %.0f ft", aircraft->has_altitude ? aircraft->altitude_ft : 0.0);
    screen_text(screen, 28, 8, line, 7);
    snprintf(line, sizeof(line), "SPD: %.0f kt", aircraft->has_speed ? aircraft->speed_kt : 0.0);
    screen_text(screen, 28, 9, line, 7);
    snprintf(line, sizeof(line), "HDG: %.0f", aircraft->has_heading ? aircraft->heading_deg : 0.0);
    screen_text(screen, 28, 10, line, 7);
    snprintf(line, sizeof(line), "V/S: %.0f ft/min", aircraft->has_vertical ? aircraft->vertical_fpm : 0.0);
    screen_text(screen, 28, 11, line, 7);
    snprintf(line, sizeof(line), "DIST: %.1f nm", distance);
    screen_text(screen, 28, 12, line, 7);
    snprintf(line, sizeof(line), "BRG: %.0f", bearing);
    screen_text(screen, 28, 13, line, 7);
    snprintf(line, sizeof(line), "SQUAWK: %s", aircraft->has_squawk ? aircraft->squawk : "-");
    screen_text(screen, 28, 14, line, 7);
    snprintf(line, sizeof(line), "AGE: %.0fs", aircraft->has_seen ? aircraft->seen_seconds : 0.0);
    screen_text(screen, 28, 15, line, 7);
    screen_text(screen, 28, 17, "ESC closes", 15);
}

static const char *key_name(int key) {
    switch (key) {
    case INPUT_QUIT: return "q";
    case INPUT_UP: return "UP";
    case INPUT_DOWN: return "DOWN";
    case INPUT_LEFT: return "LEFT";
    case INPUT_RIGHT: return "RIGHT";
    case INPUT_TAB: return "TAB";
    case INPUT_SPACE: return "SPACE";
    case INPUT_ENTER: return "ENTER";
    case INPUT_LIST: return "l";
    case INPUT_HELP: return "h";
    case INPUT_ESCAPE: return "ESC";
    case INPUT_DISCONNECT: return "DISCONNECT";
    default: return "UNKNOWN";
    }
}

static const char *action_name(int key) {
    switch (key) {
    case INPUT_QUIT: return "quit";
    case INPUT_UP: case INPUT_DOWN: case INPUT_LEFT: case INPUT_RIGHT: return "select";
    case INPUT_TAB: return "sort";
    case INPUT_SPACE: return "center";
    case INPUT_ENTER: return "details";
    case INPUT_LIST: return "list";
    case INPUT_HELP: return "help";
    default: return "none";
    }
}

static void debug_bytes(const AppConfig *config, const unsigned char *bytes, size_t length) {
    char line[256];
    size_t offset = 0;
    size_t i;
    if (config == NULL || config->debug_log_path == NULL) return;
    offset += (size_t)snprintf(line + offset, sizeof(line) - offset, "raw:");
    for (i = 0; i < length && offset + 4 < sizeof(line); ++i)
        offset += (size_t)snprintf(line + offset, sizeof(line) - offset, " %02x", bytes[i]);
    debug_event(config, line);
}

static int read_key(const AppConfig *config, DoorTransport *transport, InputDecoder *decoder,
                    int timeout_ms) {
    unsigned char bytes[64];
    size_t length = 0;
    TransportResult result;
    int key;
    key = input_decoder_feed(decoder, NULL, 0);
    if (key != INPUT_NONE) return key;
    debug_event(config, "waiting_for_input");
    result = transport_read(transport, bytes, sizeof(bytes), &length, timeout_ms);
    if (result == TRANSPORT_TIMEOUT) {
        key = input_decoder_timeout(decoder);
        return key;
    }
    if (result == TRANSPORT_WOULD_BLOCK) {
        debug_event(config, "poll readable");
        debug_event(config, "recv: EAGAIN after poll");
        return input_decoder_timeout(decoder);
    }
    if (result != TRANSPORT_OK) {
        debug_event(config, result == TRANSPORT_DISCONNECTED ? "recv: disconnected" : "recv: error");
        return INPUT_DISCONNECT;
    }
    {
        char event[64];
        debug_event(config, "poll readable");
        snprintf(event, sizeof(event), "recv: %lu bytes", (unsigned long)length);
        debug_event(config, event);
    }
    debug_bytes(config, bytes, length);
    debug_event(config, "decoder feed");
    key = input_decoder_feed(decoder, bytes, length);
    return key;
}

static volatile sig_atomic_t stop_requested;

static void on_signal(int signal_number) {
    (void)signal_number;
    stop_requested = 1;
}

static void debug_event(const AppConfig *config, const char *event) {
    FILE *file;
    if (config == NULL || config->debug_log_path == NULL) return;
    file = fopen(config->debug_log_path, "a");
    if (file == NULL) return;
    fprintf(file, "%.0f pid=%ld %.*s\n", monotonic_seconds(), (long)getpid(), 80, event);
    fclose(file);
}

int app_run(const AppConfig *config, Provider *provider, DoorTransport *transport) {
    Screen current;
    Screen previous;
    AircraftList aircraft;
    RadarState state;
    InputDecoder decoder;
    char output[128 * 1024];
    char error[256];
    char utc[16];
    int has_previous = 0;
    int details = 0;
    int help = 0;
    int paused = 0;
    int running = 1;
    double next_poll = 0.0;
    double now;
    int have_good = 0;
    double deadline = 0.0;
    int exit_code = 0;
    int disconnected = 0;
    unsigned long iteration = 0;
    void (*old_int)(int);
    void (*old_term)(int);
#ifdef SIGHUP
    void (*old_hup)(int);
#endif
    if (config == NULL || provider == NULL || transport == NULL) return 2;
    memset(&state, 0, sizeof(state));
    state.receiver_latitude = config->receiver_latitude;
    state.receiver_longitude = config->receiver_longitude;
    state.range_nm = config->range_nm > 0.0 ? config->range_nm : 100.0;
    state.show_ground = 1;
    aircraft_list_clear(&aircraft);
    input_decoder_init(&decoder);
    if (!provider->poll(provider, &aircraft, error, sizeof(error))) return 13;
    debug_event(config, transport->is_socket ? "socket opened" : "terminal opened");
    have_good = 1;
    if (config->door_mode) {
        if (config->time_left_minutes <= 0) return 15;
        deadline = monotonic_seconds() + (double)config->time_left_minutes * 60.0 - 7.0;
    }
    stop_requested = 0;
    old_int = signal(SIGINT, on_signal);
    old_term = signal(SIGTERM, on_signal);
#ifdef SIGHUP
    old_hup = signal(SIGHUP, on_signal);
#endif
    memset(state.source_status, 0, sizeof(state.source_status));
    snprintf(state.source_status, sizeof(state.source_status), "%s",
             have_good ? "OK" : "NO DATA");
    state.charset = config->charset;
    while (running) {
        {
            char event[64];
            ++iteration;
            snprintf(event, sizeof(event), "iteration=%lu", iteration);
            debug_event(config, event);
        }
        now = monotonic_seconds();
        if (stop_requested || (deadline > 0.0 && now >= deadline)) {
            if (deadline > 0.0 && now >= deadline) exit_code = 15;
            break;
        }
        if (!config->once && !paused && now >= next_poll) {
            AircraftList candidate;
            if (provider->poll(provider, &candidate, error, sizeof(error))) {
                aircraft = candidate;
                have_good = 1;
                snprintf(state.source_status, sizeof(state.source_status), "OK");
            } else {
                snprintf(state.source_status, sizeof(state.source_status),
                         have_good ? "STALE" : "SOURCE ERR");
            }
            next_poll = now + (config->refresh_seconds > 0.0 ? config->refresh_seconds : 2.0);
        }
        if (deadline > 0.0 && deadline - now <= 60.0) {
            snprintf(state.source_status, sizeof(state.source_status), "TIME LEFT");
        }
        utc_now(utc, sizeof(utc));
        radar_render(&current, &aircraft, &state, utc);
        if (details) ui_draw_details(&current, &aircraft, &state);
        if (help) ui_draw_help(&current);
        if (config->once) {
            int length = ansi_full(&current, output, sizeof(output), config->color);
            if (length > 0) transport_write(transport, (const unsigned char *)output, (size_t)length);
            running = 0;
        } else {
            int length = has_previous ? ansi_diff(&current, &previous, output, sizeof(output), config->color)
                                      : ansi_full(&current, output, sizeof(output), config->color);
            if (length > 0 && transport_write(transport, (const unsigned char *)output, (size_t)length) != TRANSPORT_OK) {
                debug_event(config, "write_error");
                disconnected = 1;
                exit_code = 14;
                break;
            }
            previous = current;
            has_previous = 1;
            {
                int key = read_key(config, transport, &decoder, 100);
                while (key != INPUT_NONE) {
                    char key_event[32];
                    if (key != INPUT_DISCONNECT) {
                        char decoded_event[64];
                        snprintf(decoded_event, sizeof(decoded_event), "decoded='%s'", key_name(key));
                        debug_event(config, decoded_event);
                    }
                    snprintf(key_event, sizeof(key_event), "action: %s", action_name(key));
                    debug_event(config, key_event);
                    switch (key) {
                    case INPUT_QUIT: running = 0; break;
                    case INPUT_DISCONNECT: debug_event(config, "read_eof"); disconnected = 1; exit_code = 14; running = 0; break;
                    case INPUT_UP: if (state.selected > 0) --state.selected; break;
                    case INPUT_DOWN: if (state.selected + 1 < aircraft.count) ++state.selected; break;
                    case INPUT_J: if (state.selected + 1 < aircraft.count) ++state.selected; break;
                    case INPUT_K: if (state.selected > 0) --state.selected; break;
                    case INPUT_PLUS: state.range_nm = state.range_nm > 5.0 ? state.range_nm / 2.0 : 5.0; break;
                    case INPUT_MINUS: state.range_nm = state.range_nm < 500.0 ? state.range_nm * 2.0 : 500.0; break;
                    case INPUT_TAB: state.sort_mode = (state.sort_mode + 1) % 3; break;
                    case INPUT_SPACE: break; /* radar is always centered on receiver */
                    case INPUT_LIST: state.list_mode = !state.list_mode; break;
                    case INPUT_HELP: help = !help; break;
                    case INPUT_QUESTION: help = !help; break;
                    case INPUT_ENTER: details = 1; break;
                    case INPUT_ESCAPE: details = 0; help = 0; break;
                    case INPUT_G: state.show_ground = !state.show_ground; break;
                    case INPUT_S: state.sort_mode = (state.sort_mode + 1) % 3; break;
                    case INPUT_P: paused = !paused; break;
                    case INPUT_R: next_poll = 0.0; break;
                    case 101: state.range_nm = 25.0; break;
                    case 102: state.range_nm = 50.0; break;
                    case 103: state.range_nm = 100.0; break;
                    case 104: state.range_nm = 200.0; break;
                    default: break;
                    }
                    if (!running) break;
                    key = input_decoder_next(&decoder);
                }
            }
        }
    }
    if (disconnected) {
        signal(SIGINT, old_int);
        signal(SIGTERM, old_term);
#ifdef SIGHUP
        signal(SIGHUP, old_hup);
#endif
        return exit_code;
    }
    ansi_stop(output, sizeof(output));
    if (transport_connected(transport)) transport_write(transport, (const unsigned char *)output, strlen(output));
    signal(SIGINT, old_int);
    signal(SIGTERM, old_term);
#ifdef SIGHUP
    signal(SIGHUP, old_hup);
#endif
    if (deadline > 0.0 && monotonic_seconds() >= deadline) return 15;
    return exit_code;
}
