#define _POSIX_C_SOURCE 200809L

#include "ansiradar80/transport.h"

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <termios.h>
#include <unistd.h>

static int wait_fd(int fd, short events, int timeout_ms) {
    struct pollfd item;
    int result;
    item.fd = fd;
    item.events = events;
    item.revents = 0;
    do { result = poll(&item, 1, timeout_ms); } while (result < 0 && errno == EINTR);
    if (result == 0) return 0;
    if (result < 0) return -1;
    if (item.revents & POLLNVAL) return -1;
    if (item.revents & (POLLIN | POLLPRI)) return 1;
    if (item.revents & (POLLERR | POLLHUP)) return -2;
    return 1;
}

TransportResult transport_read(DoorTransport *transport, unsigned char *buffer,
                               size_t capacity, size_t *bytes_read, int timeout_ms) {
    ssize_t result;
    int ready;
    if (bytes_read != NULL) *bytes_read = 0;
    if (transport == NULL || buffer == NULL || capacity == 0 || transport->read_fd < 0)
        return TRANSPORT_ERROR;
    ready = wait_fd(transport->read_fd, POLLIN, timeout_ms);
    if (ready == 0) return TRANSPORT_TIMEOUT;
    if (ready < 0) return ready == -2 ? TRANSPORT_DISCONNECTED : TRANSPORT_ERROR;
    do { result = transport->is_socket ? recv(transport->read_fd, buffer, capacity, MSG_DONTWAIT)
                                       : read(transport->read_fd, buffer, capacity); }
    while (result < 0 && errno == EINTR);
    if (result > 0) {
        if (bytes_read != NULL) *bytes_read = (size_t)result;
        return TRANSPORT_OK;
    }
    if (result == 0) return TRANSPORT_DISCONNECTED;
    if (errno == EAGAIN || errno == EWOULDBLOCK) return TRANSPORT_WOULD_BLOCK;
    if (errno == ECONNRESET || errno == ENOTCONN || errno == ECONNABORTED || errno == ESHUTDOWN)
        return TRANSPORT_DISCONNECTED;
    return TRANSPORT_ERROR;
}

TransportResult transport_write(DoorTransport *transport, const unsigned char *buffer,
                                size_t length) {
    size_t offset = 0;
    if (transport == NULL || (buffer == NULL && length != 0) || transport->write_fd < 0)
        return TRANSPORT_ERROR;
    while (offset < length) {
        ssize_t result;
        int ready = wait_fd(transport->write_fd, POLLOUT, 1000);
        if (ready == 0) continue;
        if (ready < 0) return ready == -2 ? TRANSPORT_DISCONNECTED : TRANSPORT_ERROR;
        do {
            result = transport->is_socket ? send(transport->write_fd, buffer + offset,
                                                 length - offset, MSG_NOSIGNAL)
                                          : write(transport->write_fd, buffer + offset, length - offset);
        } while (result < 0 && errno == EINTR);
        if (result > 0) { offset += (size_t)result; continue; }
        if (result < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) continue;
        if (result < 0 && (errno == EPIPE || errno == ECONNRESET || errno == ENOTCONN ||
                            errno == ECONNABORTED || errno == ESHUTDOWN)) return TRANSPORT_DISCONNECTED;
        return TRANSPORT_ERROR;
    }
    return TRANSPORT_OK;
}

int transport_connected(const DoorTransport *transport) {
    struct sockaddr_storage peer;
    socklen_t length = sizeof(peer);
    int type;
    socklen_t type_length = sizeof(type);
    if (transport == NULL || transport->read_fd < 0) return 0;
    if (!transport->is_socket) return 1;
    return getsockopt(transport->read_fd, SOL_SOCKET, SO_TYPE, &type, &type_length) == 0 &&
           type == SOCK_STREAM && getpeername(transport->read_fd, (struct sockaddr *)&peer, &length) == 0;
}

int transport_open_socket(DoorTransport *transport, int descriptor) {
    int duplicate;
    if (transport == NULL || descriptor < 0) return 0;
    duplicate = dup(descriptor);
    if (duplicate < 0) return 0;
    memset(transport, 0, sizeof(*transport));
    transport->read_fd = duplicate;
    transport->write_fd = duplicate;
    transport->owned_fd = 1;
    transport->is_socket = 1;
    if (!transport_connected(transport)) { close(duplicate); memset(transport, 0, sizeof(*transport)); transport->read_fd = -1; transport->write_fd = -1; return 0; }
    return 1;
}

int transport_open_local(DoorTransport *transport) {
    struct termios raw;
    if (transport == NULL) return 0;
    memset(transport, 0, sizeof(*transport));
    transport->read_fd = -1;
    transport->write_fd = -1;
    transport->read_fd = STDIN_FILENO;
    transport->write_fd = STDOUT_FILENO;
    transport->is_local = 1;
    if (tcgetattr(STDIN_FILENO, &raw) != 0) { transport->read_fd = STDIN_FILENO; transport->write_fd = STDOUT_FILENO; return 1; }
    transport->private_data = malloc(sizeof(struct termios));
    if (transport->private_data == NULL) return 0;
    *(struct termios *)transport->private_data = raw;
    raw.c_lflag &= (tcflag_t)~(ICANON | ECHO);
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 1;
    if (tcsetattr(STDIN_FILENO, TCSANOW, &raw) != 0) { free(transport->private_data); transport->private_data = NULL; }
    return 1;
}

int transport_local_restore(DoorTransport *transport) {
    int result = 1;
    if (transport == NULL || !transport->is_local) return 1;
    if (transport->private_data != NULL) {
        result = tcsetattr(STDIN_FILENO, TCSANOW, (struct termios *)transport->private_data) == 0;
        free(transport->private_data);
        transport->private_data = NULL;
    }
    return result;
}

void transport_close(DoorTransport *transport) {
    if (transport == NULL) return;
    transport_local_restore(transport);
    if (transport->owned_fd && transport->read_fd >= 0) close(transport->read_fd);
    transport->read_fd = -1;
    transport->write_fd = -1;
    transport->owned_fd = 0;
}
