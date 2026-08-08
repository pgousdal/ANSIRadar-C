#ifndef ANSIRADAR80_DOOR32_H
#define ANSIRADAR80_DOOR32_H

#include <stddef.h>

typedef struct {
    int communication_type;
    int communication_handle;
    long baud_rate;
    char bbs_id[64];
    long user_id;
    char user_name[64];
    char user_alias[64];
    int security_level;
    int time_left_minutes;
    int terminal_emulation;
    int node_number;
} Door32Info;

enum Door32Result {
    DOOR32_OK = 0,
    DOOR32_OPEN_ERROR,
    DOOR32_INVALID,
    DOOR32_UNSUPPORTED
};

int door32_parse(const char *path, Door32Info *info, char *error, size_t error_size);

#endif
