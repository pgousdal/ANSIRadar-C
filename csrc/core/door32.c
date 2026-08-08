#include "ansiradar80/door32.h"

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void set_error(char *error, size_t size, const char *text) {
    if (error != NULL && size > 0) snprintf(error, size, "%s", text);
}

static int read_line(FILE *file, char *line, size_t size) {
    size_t length;
    int c;
    if (fgets(line, (int)size, file) == NULL) return 0;
    length = strlen(line);
    if (length == 0 || line[length - 1] == '\n') {
        if (length > 0 && line[length - 1] == '\n') line[--length] = '\0';
        if (length > 0 && line[length - 1] == '\r') line[--length] = '\0';
        return 1;
    }
    c = fgetc(file);
    if (c != '\n' && c != EOF) return -1;
    if (length > 0 && line[length - 1] == '\r') line[--length] = '\0';
    return c == EOF || c == '\n' ? 1 : -1;
}

static int number(const char *text, long *value) {
    char *end;
    long parsed;
    if (text == NULL || *text == '\0') return 0;
    errno = 0;
    parsed = strtol(text, &end, 10);
    if (errno == ERANGE || end == text || *end != '\0') return 0;
    *value = parsed;
    return 1;
}

static void sanitize(char *text) {
    unsigned char *cursor = (unsigned char *)text;
    while (*cursor != '\0') {
        if (*cursor < 32 || *cursor == 127) *cursor = '?';
        ++cursor;
    }
}

static void copy_field(char *destination, size_t capacity, const char *source) {
    if (capacity == 0) return;
    strncpy(destination, source, capacity - 1);
    destination[capacity - 1] = '\0';
}

int door32_parse(const char *path, Door32Info *info, char *error, size_t error_size) {
    FILE *file;
    char lines[11][256];
    char extra[2];
    long values[8];
    int i;
    int result = DOOR32_INVALID;
    if (info == NULL || path == NULL) {
        set_error(error, error_size, "invalid DOOR32 arguments");
        return DOOR32_INVALID;
    }
    file = fopen(path, "rb");
    if (file == NULL) {
        set_error(error, error_size, "cannot open DOOR32.SYS");
        return DOOR32_OPEN_ERROR;
    }
    memset(info, 0, sizeof(*info));
    for (i = 0; i < 11; ++i) {
        int line_result = read_line(file, lines[i], sizeof(lines[i]));
        if (line_result <= 0) {
            set_error(error, error_size, line_result < 0 ? "DOOR32 line is too long" : "DOOR32.SYS is incomplete");
            goto done;
        }
    }
    if (fread(extra, 1, 1, file) != 0) {
        set_error(error, error_size, "DOOR32.SYS has extra content");
        goto done;
    }
    for (i = 0; i < 11; ++i) sanitize(lines[i]);
    if (!number(lines[0], &values[0]) || !number(lines[1], &values[1]) ||
        !number(lines[2], &values[2]) || !number(lines[4], &values[3]) ||
        !number(lines[7], &values[4]) || !number(lines[8], &values[5]) ||
        !number(lines[9], &values[6]) || !number(lines[10], &values[7])) {
        set_error(error, error_size, "DOOR32.SYS contains an invalid integer");
        goto done;
    }
    if (values[1] < 0 || values[1] > INT_MAX || values[4] < INT_MIN || values[4] > INT_MAX ||
        values[5] < INT_MIN || values[5] > INT_MAX || values[6] < INT_MIN || values[6] > INT_MAX ||
        values[7] < INT_MIN || values[7] > INT_MAX) {
        set_error(error, error_size, "DOOR32 integer is out of range");
        goto done;
    }
    if (values[0] != 2) {
        set_error(error, error_size, "unsupported DOOR32 communication type");
        result = DOOR32_UNSUPPORTED;
        goto done;
    }
    info->communication_type = (int)values[0];
    info->communication_handle = (int)values[1];
    info->baud_rate = values[2];
    copy_field(info->bbs_id, sizeof(info->bbs_id), lines[3]);
    info->user_id = values[3];
    copy_field(info->user_name, sizeof(info->user_name), lines[5]);
    copy_field(info->user_alias, sizeof(info->user_alias), lines[6]);
    info->security_level = (int)values[4];
    info->time_left_minutes = (int)values[5];
    info->terminal_emulation = (int)values[6];
    info->node_number = (int)values[7];
    result = DOOR32_OK;
done:
    fclose(file);
    return result;
}
