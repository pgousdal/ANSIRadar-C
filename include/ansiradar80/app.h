#ifndef ANSIRADAR80_APP_H
#define ANSIRADAR80_APP_H

#include "provider.h"
#include "transport.h"

typedef struct {
    const char *source_kind;
    const char *source_path;
    double receiver_latitude;
    double receiver_longitude;
    double range_nm;
    int once;
    int color;
    int width;
    int height;
    int charset;
    double refresh_seconds;
    const char *debug_log_path;
    const char *door32_path;
    int door_mode;
    int time_left_minutes;
} AppConfig;

int app_run(const AppConfig *config, Provider *provider, DoorTransport *transport);

#endif
