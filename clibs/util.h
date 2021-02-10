#ifndef CLIBS_UTIL_H_
#define CLIBS_UTIL_H_

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/times.h>
#include <sys/types.h>
#include <unistd.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifdef DEBUG
#define p_debug printf

#define DBG(fmt, args...)                                              \
    do {                                                               \
        printf("[%s():%d] " fmt "\n", __FUNCTION__, __LINE__, ##args); \
    } while (0)

#define FDBG(f, fmt, args...)                                              \
    do {                                                                   \
        fprintf(f, "[%s():%d] " fmt "\n", __FUNCTION__, __LINE__, ##args); \
    } while (0)

#else
#define p_debug(...)
#define DBG(fmt, args...)
#define FDBG(f, fmt, args...)
#endif

#define ERR(fmt, args...)                                              \
    do {                                                               \
        printf("[%s():%d] " fmt "\n", __FUNCTION__, __LINE__, ##args); \
        perror(NULL);                                                  \
    } while (0)

#define ENTER() DBG("Enter")
#define EXIT() DBG("Exit");

#define MALLOC(n) calloc(1, (n))

#define ENTER_FUNC()                             \
    do {                                         \
        p_debug("\nEnter %s()\n", __FUNCTION__); \
    } while (0)

#define EXIT_FUNC()                           \
    do {                                      \
        p_debug("Exit %s()\n", __FUNCTION__); \
    } while (0)

#define safe_free(p)       \
    do {                   \
        if ((p) != NULL) { \
            free((p));     \
            (p) = NULL;    \
        }                  \
    } while (0)

#define safe_fclose(f)     \
    do {                   \
        if ((f) != NULL) { \
            fclose((f));   \
            (f) = NULL;    \
        }                  \
    } while (0)

#define safe_close(fd)   \
    do {                 \
        if ((fd) > 0) {  \
            close((fd)); \
            (fd) = -1;   \
        }                \
    } while (0)

#define MEMSET(o) memset(&(o), 0, sizeof(o))
#define MEMSET_P(p) memset(p, 0, sizeof(*(p)))

int set_nonblock(int fd);
int Read(int fd, void *data, size_t size);
int Write(int fd, void *data, size_t len);

int8_t is_ipv6(const char *ip);
int tcp_server_create(const char *ip_str, uint16_t port, int backlog);
int tcp_client_create(const char *ip_str, uint16_t port, int8_t *is_connected);
int udp_server_create(const char *ip_str, uint16_t port);
int udp_client_create(const char *ip_str, uint16_t port);
int unix_tcp_server_create(const char *path, int backlog);
int unix_tcp_client_create(const char *path);
int unix_udp_client_create(const char *path);
int unix_udp_server_create(const char *path);
int Accept(int sock, struct sockaddr *cli_addr, socklen_t *addrlen, int flags);

#ifdef __cplusplus
}
#endif

#endif  // CLIBS_EPOLL_H_
