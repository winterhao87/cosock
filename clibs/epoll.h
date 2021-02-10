#ifndef CLIBS_EPOLL_H_
#define CLIBS_EPOLL_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <sys/epoll.h>

typedef struct epoll_fd {
    int epfd;
    size_t size;
    struct epoll_event *events;
} epoll_fd_t;

int epoll_fd_create(epoll_fd_t *self, size_t size);
int epoll_fd_release(epoll_fd_t *self);

int epoll_fd_add(epoll_fd_t *self, int fd, int event, void *ptr);
int epoll_fd_del(epoll_fd_t *self, int fd);
int epoll_fd_mod(epoll_fd_t *self, int fd, int event, void *ptr);

int epoll_fd_wait(epoll_fd_t *self, int timeout);
struct epoll_event *epoll_events(epoll_fd_t *self);

#ifdef __cplusplus
}
#endif

#endif  // CLIBS_EPOLL_H_
