#define _POSIX_C_SOURCE 200809L

#include "ansiradar80/app.h"
#include "ansiradar80/door32.h"
#include "ansiradar80/input.h"
#include "ansiradar80/transport.h"

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>

static char door_binary[512] = "./ansiradar80";

static void make_dropfile(const char *path, const char *ending) {
    FILE *file = fopen(path, "wb");
    assert(file != NULL);
    fputs("2\n", file); fputs("0\n", file); fputs("57600\n", file);
    fputs("BBS\n", file); fputs("42\n", file); fputs("Name\n", file);
    fputs("Alias\n", file); fputs("10\n", file); fputs("2\n", file);
    fputs("1\n", file); fputs("3", file); fputs(ending, file);
    fclose(file);
}

static int contains_bytes(const char *buffer, size_t length, const char *needle) {
    size_t i;
    size_t needle_length = strlen(needle);
    if (needle_length > length) return 0;
    for (i = 0; i + needle_length <= length; ++i)
        if (memcmp(buffer + i, needle, needle_length) == 0) return 1;
    return 0;
}

static void test_doorfile(void) {
    char path[128];
    char error[128];
    Door32Info info;
    int result;
    snprintf(path, sizeof(path), "/tmp/ansiradar-door-%ld", (long)getpid());
    make_dropfile(path, "\r\n");
    result = door32_parse(path, &info, error, sizeof(error));
    assert(result == DOOR32_OK && info.communication_handle == 0 && info.time_left_minutes == 2);
    make_dropfile(path, "");
    assert(door32_parse(path, &info, error, sizeof(error)) == DOOR32_OK);
    {
        FILE *file = fopen(path, "wb");
        assert(file != NULL);
        fputs("2\n0\n1\nB\001S\n1\nN\nA\n1\n1\n1\n1", file);
        fclose(file);
        assert(door32_parse(path, &info, error, sizeof(error)) == DOOR32_OK);
        assert(strcmp(info.bbs_id, "B?S") == 0);
    }
    {
        FILE *file = fopen(path, "wb");
        assert(file != NULL);
        fputs("2\n0\n1\nBBS\n1\nN\nA\n1\n1\n1\n1\nextra\n", file);
        fclose(file);
    }
    assert(door32_parse(path, &info, error, sizeof(error)) == DOOR32_INVALID);
    {
        FILE *file = fopen(path, "wb");
        int i;
        assert(file != NULL);
        for (i = 0; i < 300; ++i) fputc('x', file);
        fputc('\n', file);
        fclose(file);
        assert(door32_parse(path, &info, error, sizeof(error)) == DOOR32_INVALID);
    }
    make_dropfile(path, "");
    {
        FILE *file = fopen(path, "wb");
        assert(file != NULL);
        fputs("7\n0\n1\nBBS\n1\nN\nA\n1\n1\n1\n1", file);
        fclose(file);
        assert(door32_parse(path, &info, error, sizeof(error)) == DOOR32_UNSUPPORTED);
    }
    {
        FILE *file = fopen(path, "wb");
        assert(file != NULL);
        fputs("2\n0\n", file);
        fclose(file);
        assert(door32_parse(path, &info, error, sizeof(error)) == DOOR32_INVALID);
    }
    unlink(path);
}

static void test_transport(void) {
    int pair[2];
    int flags;
    unsigned char buffer[8];
    size_t length;
    DoorTransport transport;
    assert(!transport_open_socket(&transport, -1));
    assert(socketpair(AF_UNIX, SOCK_STREAM, 0, pair) == 0);
    flags = fcntl(pair[0], F_GETFL, 0);
    assert(flags >= 0 && (flags & O_NONBLOCK) == 0);
    assert(transport_open_socket(&transport, pair[0]));
    assert(fcntl(pair[0], F_GETFL, 0) == flags);
    assert(transport_read(&transport, buffer, sizeof(buffer), &length, 10) == TRANSPORT_TIMEOUT);
    assert(write(pair[1], "abc", 3) == 3);
    assert(transport_read(&transport, buffer, sizeof(buffer), &length, 1000) == TRANSPORT_OK);
    assert(length == 3 && memcmp(buffer, "abc", 3) == 0);
    assert(transport_write(&transport, (const unsigned char *)"z", 1) == TRANSPORT_OK);
    assert(read(pair[1], buffer, sizeof(buffer)) == 1 && buffer[0] == 'z');
    close(pair[1]);
    assert(transport_read(&transport, buffer, sizeof(buffer), &length, 1000) == TRANSPORT_DISCONNECTED);
    transport_close(&transport);
    assert(fcntl(pair[0], F_GETFL, 0) == flags);
    close(pair[0]);
}

static void test_shared_socket_reader_race(void) {
    int pair[2];
    int controls[2][2];
    int results[2][2];
    pid_t children[2];
    int reader;
    unsigned char signal_byte;
    unsigned char result;
    unsigned char buffer[1];
    struct pollfd item;
    assert(socketpair(AF_UNIX, SOCK_STREAM, 0, pair) == 0);
    for (reader = 0; reader < 2; ++reader) {
        assert(pipe(controls[reader]) == 0);
        assert(pipe(results[reader]) == 0);
        children[reader] = fork();
        assert(children[reader] >= 0);
        if (children[reader] == 0) {
            ssize_t length;
            close(pair[1]);
            close(controls[reader][1]);
            close(results[reader][0]);
            item.fd = pair[0];
            item.events = POLLIN;
            item.revents = 0;
            assert(poll(&item, 1, 1000) == 1 && (item.revents & POLLIN) != 0);
            signal_byte = 'p';
            assert(write(results[reader][1], &signal_byte, 1) == 1);
            assert(read(controls[reader][0], &signal_byte, 1) == 1);
            errno = 0;
            length = recv(pair[0], buffer, sizeof(buffer), MSG_DONTWAIT);
            if (length == 1 && buffer[0] == 'x') result = 1;
            else if (length < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) result = 2;
            else result = 3;
            assert(write(results[reader][1], &result, 1) == 1);
            _exit(0);
        }
        close(controls[reader][0]);
        close(results[reader][1]);
    }
    close(pair[0]);
    assert(write(pair[1], "x", 1) == 1);
    for (reader = 0; reader < 2; ++reader)
        assert(read(results[reader][0], &signal_byte, 1) == 1 && signal_byte == 'p');
    assert(write(controls[0][1], "r", 1) == 1);
    assert(read(results[0][0], &result, 1) == 1 && result == 1);
    assert(write(controls[1][1], "r", 1) == 1);
    assert(read(results[1][0], &result, 1) == 1 && result == 2);
    for (reader = 0; reader < 2; ++reader) {
        int status;
        close(controls[reader][1]);
        close(results[reader][0]);
        assert(waitpid(children[reader], &status, 0) == children[reader]);
    }
    close(pair[1]);
}

static void test_input(void) {
    InputDecoder decoder;
    const unsigned char up[] = {27, '[', 'A'};
    const unsigned char down[] = {27, '[', 'B'};
    const unsigned char left[] = {27, '[', 'D'};
    const unsigned char right[] = {27, '[', 'C'};
    input_decoder_init(&decoder);
    assert(input_decoder_feed(&decoder, (const unsigned char *)"q", 1) == INPUT_QUIT);
    assert(input_decoder_feed(&decoder, (const unsigned char *)"l", 1) == INPUT_LIST);
    assert(input_decoder_feed(&decoder, (const unsigned char *)"h", 1) == INPUT_HELP);
    assert(input_decoder_feed(&decoder, (const unsigned char *)"\t", 1) == INPUT_TAB);
    assert(input_decoder_feed(&decoder, (const unsigned char *)" ", 1) == INPUT_SPACE);
    assert(input_decoder_feed(&decoder, (const unsigned char *)"\r", 1) == INPUT_ENTER);
    assert(input_decoder_feed(&decoder, (const unsigned char *)"abc", 3) == INPUT_NONE);
    assert(input_decoder_feed(&decoder, NULL, 0) == INPUT_NONE);
    assert(input_decoder_feed(&decoder, NULL, 0) == INPUT_NONE);
    input_decoder_init(&decoder);
    assert(input_decoder_feed(&decoder, (const unsigned char *)"H", 1) == INPUT_HELP);
    assert(input_decoder_timeout(&decoder) == INPUT_NONE);
    assert(input_decoder_feed(&decoder, (const unsigned char *)"L", 1) == INPUT_LIST);
    assert(input_decoder_feed(&decoder, (const unsigned char *)" ", 1) == INPUT_SPACE);
    assert(input_decoder_feed(&decoder, (const unsigned char *)"\t", 1) == INPUT_TAB);
    assert(input_decoder_feed(&decoder, (const unsigned char *)"\n", 1) == INPUT_ENTER);
    input_decoder_init(&decoder);
    assert(input_decoder_feed(&decoder, (const unsigned char *)"HLQ", 3) == INPUT_HELP);
    assert(input_decoder_next(&decoder) == INPUT_LIST);
    assert(input_decoder_next(&decoder) == INPUT_QUIT);
    assert(input_decoder_next(&decoder) == INPUT_NONE);
    input_decoder_init(&decoder);
    {
        const unsigned char negotiation_and_key[] = {0xff, 0xfb, 1, 'Q'};
        assert(input_decoder_feed(&decoder, negotiation_and_key,
                                  sizeof(negotiation_and_key)) == INPUT_QUIT);
    }
    input_decoder_init(&decoder);
    assert(input_decoder_feed(&decoder, up, 1) == INPUT_NONE);
    assert(input_decoder_feed(&decoder, up + 1, 1) == INPUT_NONE);
    assert(input_decoder_feed(&decoder, up + 2, 1) == INPUT_UP);
    assert(input_decoder_feed(&decoder, down, sizeof(down)) == INPUT_DOWN);
    assert(input_decoder_feed(&decoder, left, sizeof(left)) == INPUT_LEFT);
    assert(input_decoder_feed(&decoder, right, sizeof(right)) == INPUT_RIGHT);
    input_decoder_init(&decoder);
    assert(input_decoder_feed(&decoder, (const unsigned char *)"\033", 1) == INPUT_NONE);
    assert(input_decoder_feed(&decoder, (const unsigned char *)"[", 1) == INPUT_NONE);
    assert(input_decoder_feed(&decoder, (const unsigned char *)"A", 1) == INPUT_UP);
    input_decoder_init(&decoder);
    assert(input_decoder_feed(&decoder, (const unsigned char *)"\xff\xfb", 2) == INPUT_NONE);
    assert(input_decoder_feed(&decoder, (const unsigned char *)"\001", 1) == INPUT_NONE);
    assert(input_decoder_feed(&decoder, (const unsigned char *)"q", 1) == INPUT_QUIT);
    input_decoder_init(&decoder);
    assert(input_decoder_feed(&decoder, (const unsigned char *)"\033", 1) == INPUT_NONE);
    assert(input_decoder_timeout(&decoder) == INPUT_NONE);
    assert(input_decoder_timeout(&decoder) == INPUT_ESCAPE);
    input_decoder_init(&decoder);
    assert(input_decoder_feed(&decoder, (const unsigned char *)"\033[99", 5) == INPUT_NONE);
    assert(input_decoder_feed(&decoder, (const unsigned char *)"~q", 2) == INPUT_QUIT);
    assert(input_decoder_next(&decoder) == INPUT_NONE);
}

static void test_runtime(void) {
    int pairs[2][2];
    pid_t children[2];
    int session;
    char buffer[4096];
    ssize_t length;
    for (session = 0; session < 2; ++session) assert(socketpair(AF_UNIX, SOCK_STREAM, 0, pairs[session]) == 0);
    for (session = 0; session < 2; ++session) {
        children[session] = fork();
        assert(children[session] >= 0);
        if (children[session] == 0) {
        AppConfig config;
        Provider provider;
        DoorTransport transport;
        char error[128];
        memset(&config, 0, sizeof(config));
        memset(&provider, 0, sizeof(provider));
        assert(provider_readsb_file_create(&provider, "fixtures80/readsb.json", error, sizeof(error)));
        config.source_kind = "readsb";
        config.source_path = "fixtures80/readsb.json";
        config.receiver_latitude = 58.3405;
        config.receiver_longitude = 6.2812;
        config.range_nm = 100.0;
        config.width = 80;
        config.height = 25;
        config.color = 0;
        config.charset = 1;
        config.refresh_seconds = 1.0;
        assert(transport_open_socket(&transport, pairs[session][1]));
        close(pairs[session][0]);
        (void)app_run(&config, &provider, &transport);
        transport_close(&transport);
        provider_destroy(&provider);
        _exit(0);
        }
        close(pairs[session][1]);
    }
    for (session = 0; session < 2; ++session) {
        length = read(pairs[session][0], buffer, sizeof(buffer));
        assert(length > 0);
        assert(contains_bytes(buffer, (size_t)length, "\033[2J"));
        assert(write(pairs[session][0], "q", 1) == 1);
    }
    for (session = 0; session < 2; ++session) {
        int status;
        assert(waitpid(children[session], &status, 0) == children[session] && WIFEXITED(status));
        close(pairs[session][0]);
    }
}

static void test_final_binary(void) {
    int pair[2];
    pid_t child;
    int status;
    char path[128];
    char debug_path[128];
    char buffer[4096];
    char log[8192];
    ssize_t length;
    FILE *file;
    assert(socketpair(AF_UNIX, SOCK_STREAM, 0, pair) == 0);
    snprintf(path, sizeof(path), "/tmp/ansiradar-door-exec-%ld", (long)getpid());
    snprintf(debug_path, sizeof(debug_path), "/tmp/ansiradar-door-log-%ld", (long)getpid());
    unlink(debug_path);
    file = fopen(path, "wb");
    assert(file != NULL);
    fprintf(file, "2\n%d\n57600\nBBS\n42\nName\nAlias\n10\n2\n1\n1\n", pair[1]);
    fclose(file);
    child = fork();
    assert(child >= 0);
    if (child == 0) {
        close(pair[0]);
        execl(door_binary, door_binary, "--door32", path, "--file",
              "fixtures80/readsb.json", "--receiver-lat", "58.3405",
              "--receiver-lon", "6.2812", "--color", "never", "--debug-log",
              debug_path, (char *)NULL);
        _exit(127);
    }
    close(pair[1]);
    length = read(pair[0], buffer, sizeof(buffer));
    assert(length > 0 && contains_bytes(buffer, (size_t)length, "\033[2J"));
    assert(write(pair[0], "HQ", 2) == 2);
    assert(waitpid(child, &status, 0) == child && WIFEXITED(status) && WEXITSTATUS(status) == 0);
    file = fopen(debug_path, "rb");
    assert(file != NULL);
    length = (ssize_t)fread(log, 1, sizeof(log) - 1, file);
    fclose(file);
    assert(length > 0);
    log[length] = '\0';
    assert(strstr(log, "decoded='h'") != NULL);
    assert(strstr(log, "action: help") != NULL);
    assert(strstr(log, "action: quit") != NULL);
    assert(strstr(log, "action: help") < strstr(log, "action: quit"));
    close(pair[0]);
    unlink(path);
    unlink(debug_path);
}

int main(int argc, char **argv) {
    const char *slash;
    if (argc > 0 && argv[0] != NULL && (slash = strrchr(argv[0], '/')) != NULL) {
        size_t directory_length = (size_t)(slash - argv[0]);
        if (directory_length + strlen("/ansiradar80") + 1 < sizeof(door_binary)) {
            memcpy(door_binary, argv[0], directory_length);
            strcpy(door_binary + directory_length, "/ansiradar80");
        }
    }
    test_doorfile();
    test_transport();
    test_shared_socket_reader_race();
    test_input();
    test_runtime();
    test_final_binary();
    return 0;
}
