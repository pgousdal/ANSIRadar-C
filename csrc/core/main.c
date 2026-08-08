#include "ansiradar80/app.h"
#include "ansiradar80/door32.h"
#include "ansiradar80/transport.h"
#include "ansiradar80/version.h"

#include <stdio.h>
#include <errno.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

static void usage(const char *program) {
    printf("Usage: %s [options]\n", program);
    puts("  --source readsb|dump1090|replay");
    puts("  --file PATH              JSON snapshot or CSV replay");
    puts("  --receiver-lat FLOAT     receiver latitude");
    puts("  --receiver-lon FLOAT     receiver longitude");
    puts("  --range NM               radar range (default 100)");
    puts("  --once                   render one deterministic frame");
    puts("  --color always|never     ANSI color policy");
    puts("  --charset ascii|cp437    display character profile");
    puts("  --refresh SECONDS        source refresh interval (default 2)");
    puts("  --door32 PATH            Mystic DOOR32.SYS descriptor");
    puts("  --debug-log PATH         append file-only diagnostics");
    puts("  --width 80 --height 25   classic BBS dimensions");
    puts("  --help                   show this help");
    puts("  --version                show project version");
}

static int parse_double(const char *text, double *value) {
    char *end;
    double parsed;
    errno = 0;
    parsed = strtod(text, &end);
    if (errno == ERANGE || end == text || *end != '\0' || !isfinite(parsed)) return 0;
    *value = parsed;
    return 1;
}

int main(int argc, char **argv) {
    AppConfig config;
    Provider provider;
    DoorTransport transport;
    Door32Info door;
    char error[256];
    int i;
    memset(&config, 0, sizeof(config));
    memset(&provider, 0, sizeof(provider));
    memset(&transport, 0, sizeof(transport));
    transport.read_fd = -1;
    transport.write_fd = -1;
    config.source_kind = "readsb";
    config.range_nm = 100.0;
    config.width = 80;
    config.height = 25;
    config.color = 1;
    config.charset = 1;
    config.refresh_seconds = 2.0;
    for (i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--help") == 0) {
            usage(argv[0]);
            return 0;
        } else if (strcmp(argv[i], "--version") == 0) {
            puts(ANSIRADAR_VERSION);
            return 0;
        } else if (strcmp(argv[i], "--once") == 0) {
            config.once = 1;
        } else if (strcmp(argv[i], "--source") == 0 && i + 1 < argc) {
            config.source_kind = argv[++i];
        } else if (strcmp(argv[i], "--file") == 0 && i + 1 < argc) {
            config.source_path = argv[++i];
        } else if (strcmp(argv[i], "--receiver-lat") == 0 && i + 1 < argc) {
            if (!parse_double(argv[++i], &config.receiver_latitude)) return 2;
        } else if (strcmp(argv[i], "--receiver-lon") == 0 && i + 1 < argc) {
            if (!parse_double(argv[++i], &config.receiver_longitude)) return 2;
        } else if (strcmp(argv[i], "--range") == 0 && i + 1 < argc) {
            if (!parse_double(argv[++i], &config.range_nm)) return 2;
        } else if (strcmp(argv[i], "--width") == 0 && i + 1 < argc) {
            config.width = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--height") == 0 && i + 1 < argc) {
            config.height = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--color") == 0 && i + 1 < argc) {
            ++i;
            if (strcmp(argv[i], "always") != 0 && strcmp(argv[i], "never") != 0) return 2;
            config.color = strcmp(argv[i], "never") != 0;
        } else if (strcmp(argv[i], "--charset") == 0 && i + 1 < argc) {
            ++i;
            if (strcmp(argv[i], "ascii") != 0 && strcmp(argv[i], "cp437") != 0) return 2;
            config.charset = strcmp(argv[i], "ascii") == 0 ? 0 : 1;
        } else if (strcmp(argv[i], "--refresh") == 0 && i + 1 < argc) {
            if (!parse_double(argv[++i], &config.refresh_seconds)) return 2;
        } else if (strcmp(argv[i], "--door32") == 0 && i + 1 < argc) {
            config.door_mode = 1;
            config.door32_path = argv[++i];
        } else if (strcmp(argv[i], "--debug-log") == 0 && i + 1 < argc) {
            config.debug_log_path = argv[++i];
        } else {
            fprintf(stderr, "unknown or incomplete option: %s\n", argv[i]);
            return 2;
        }
    }
    if (config.source_path == NULL || config.receiver_latitude < -90.0 ||
        config.receiver_latitude > 90.0 || config.receiver_longitude < -180.0 ||
        config.receiver_longitude > 180.0 || config.range_nm <= 0.0 ||
        config.width != 80 || config.height != 25 || config.refresh_seconds <= 0.0) {
        fprintf(stderr, "--file, valid receiver coordinates, and 80x25 are required\n");
        return 2;
    }
    if (config.door_mode) {
        int door_result = door32_parse(config.door32_path, &door, error, sizeof(error));
        if (door_result != DOOR32_OK) {
            fprintf(stderr, "%s\n", error);
            return door_result == DOOR32_UNSUPPORTED ? 11 : 10;
        }
        config.time_left_minutes = door.time_left_minutes;
    }
    if (strcmp(config.source_kind, "readsb") == 0) {
        if (!provider_readsb_file_create(&provider, config.source_path, error, sizeof(error))) {
            fprintf(stderr, "%s\n", error);
            return config.door_mode ? 13 : 3;
        }
    } else if (strcmp(config.source_kind, "dump1090") == 0) {
        if (!provider_dump1090_file_create(&provider, config.source_path, error, sizeof(error))) {
            fprintf(stderr, "%s\n", error);
            return config.door_mode ? 13 : 3;
        }
    } else if (strcmp(config.source_kind, "replay") == 0) {
        if (!provider_replay_csv_create(&provider, config.source_path, error, sizeof(error))) {
            fprintf(stderr, "%s\n", error);
            return config.door_mode ? 13 : 3;
        }
    } else {
        fprintf(stderr, "unsupported provider: %s\n", config.source_kind);
        return 2;
    }
    if (config.door_mode) {
        if (!transport_open_socket(&transport, door.communication_handle)) {
            fprintf(stderr, "invalid DOOR32 socket descriptor\n");
            provider_destroy(&provider);
            return 12;
        }
    } else if (!transport_open_local(&transport)) {
        fprintf(stderr, "cannot initialize local terminal\n");
        provider_destroy(&provider);
        return 2;
    }
    i = app_run(&config, &provider, &transport);
    transport_close(&transport);
    provider_destroy(&provider);
    return i;
}
