#include "epoll.h"

#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>

#include "util.h"

#define EPOLL_DEFAULT_SIZE (1024)

int epoll_fd_create(epoll_fd_t *self, size_t size) {
    if (!size) size = EPOLL_DEFAULT_SIZE;

    int epfd = epoll_create(size);
    if (epfd < 0) return -1;

    self->size = size;
    self->events = (struct epoll_event *)malloc(sizeof(struct epoll_event) * size);
    if (self->events == NULL) {
        close(epfd);
        return -1;
    }

    self->epfd = epfd;
    return 0;
}

void epoll_fd_close(epoll_fd_t *self) {
    assert(self);
    if (self && self->events) {
        free(self->events);
        self->events = NULL;
        self->size = 0;
        close(self->epfd);
    }
}

int epoll_fd_add(epoll_fd_t *self, int fd, int event, void *ptr) {
    if (set_nonblock(fd) < 0) {
        ERR("set_nonblock fd=%d fail", fd);
        return -1;
    }

    struct epoll_event ev;
    if (ptr)
        ev.data.ptr = ptr;
    else
        ev.data.fd = fd;
    ev.events = event;

    if (epoll_ctl(self->epfd, EPOLL_CTL_ADD, fd, &ev) < 0) {
        ERR("epoll_ctl_add fd=%d, event=%d fail", fd, event);
        return -1;
    }

    DBG("epoll_ctl_add fd=%d, event=%d, ptr=%p", fd, event, ptr);
    return 0;
}

int epoll_fd_mod(epoll_fd_t *self, int fd, int event, void *ptr) {
    struct epoll_event ev;
    if (ptr)
        ev.data.ptr = ptr;
    else
        ev.data.fd = fd;
    ev.events = event;

    if (epoll_ctl(self->epfd, EPOLL_CTL_MOD, fd, &ev) < 0) {
        ERR("epoll_ctl_mod fd=%d, event=%d fail", fd, event);
        return -1;
    }

    DBG("epoll_ctl_mod fd=%d, event=%d", fd, event);
    return 0;
}

int epoll_fd_del(epoll_fd_t *self, int fd) {
    if (epoll_ctl(self->epfd, EPOLL_CTL_DEL, fd, NULL) < 0) {
        ERR("epoll_ctl_del fd=%d fail", fd);
        return -1;
    }

    DBG("epoll_ctl_del fd=%d", fd);
    return 0;
}

int epoll_fd_wait(epoll_fd_t *self, int timeout) { return epoll_wait(self->epfd, self->events, self->size, timeout); }

struct epoll_event *epoll_events(epoll_fd_t *self) {
    return self->events;
}
