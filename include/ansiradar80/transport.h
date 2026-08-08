#ifndef ANSIRADAR80_TRANSPORT_H
#define ANSIRADAR80_TRANSPORT_H

#include <stddef.h>

typedef struct DoorTransport DoorTransport;

typedef enum {
    TRANSPORT_OK = 0,
    TRANSPORT_TIMEOUT,
    TRANSPORT_WOULD_BLOCK,
    TRANSPORT_DISCONNECTED,
    TRANSPORT_ERROR
} TransportResult;

struct DoorTransport {
    int read_fd;
    int write_fd;
    int owned_fd;
    int is_socket;
    int is_local;
    void *private_data;
};

TransportResult transport_read(DoorTransport *transport, unsigned char *buffer,
                               size_t capacity, size_t *bytes_read, int timeout_ms);
TransportResult transport_write(DoorTransport *transport, const unsigned char *buffer,
                                size_t length);
int transport_connected(const DoorTransport *transport);
void transport_close(DoorTransport *transport);
int transport_open_socket(DoorTransport *transport, int descriptor);
int transport_open_local(DoorTransport *transport);
int transport_local_restore(DoorTransport *transport);

#endif
