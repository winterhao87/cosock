#include "util.h"

int set_nonblock(int fd) {
    int val;
    if ((val = fcntl(fd, F_GETFL, 0)) == -1) {
        perror("fcntl get");
        return -1;
    }

    if (val & O_NONBLOCK) return 0;

    val |= O_NONBLOCK;
    if (fcntl(fd, F_SETFL, val) == -1) {
        perror("fcntl set");
        return -1;
    }

    return 0;
}

// read until timeout, error, specific size.
int Read(int fd, void *data, size_t size) {
    if (fd < 0 || data == NULL) {
        DBG("invalid args");
        return -1;
    }

    if (size == 0) return 0;

    ssize_t ret = -1;
_AGAIN:
    ret = read(fd, data, size);
    if (ret < 0) {
        if (errno == EINTR) goto _AGAIN;

        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            DBG("EAGAIN");
            return -EAGAIN;
        }

        return -1;
    }

    return ret;
}

int Write(int fd, void *data, size_t len) {
    if (fd < 0 || data == NULL) {
        DBG("invalid args");
        return -1;
    }

    if (len == 0) return 0;

    ssize_t ret = -1;
_AGAIN:
    ret = write(fd, data, len);
    if (ret < 0) {
        if (errno == EINTR) goto _AGAIN;

        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            DBG("EAGAIN");
            return 0;
        }

        return -1;
    }

    return ret;
}
