#ifndef CLIBS_SOCK_H_
#define CLIBS_SOCK_H_

#include "util.h"
#include "sock.h"

#include <sys/uio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define SOCKET_TYPE_MASK 0xf0

typedef enum {
    SOCK_UNKONW_TYPE = 0,
    SOCK_TCP = 0x10,
    SOCK_TCP_SERVER,
    SOCK_TCP_CLIENT,
    SOCK_UDP = 0x20,
    SOCK_UDP_SERVER,
    SOCK_UDP_CLIENT,
    SOCK_UNIX = 0x30,
    SOCK_UNIX_SERVER,
    SOCK_UNIX_CLIENT
    
} sock_type_t;

struct netaddr
{
    char ip[64];
    uint16_t port;
};

typedef struct sock
{
    int fd;
    int domain;
    sock_type_t type;
    union{
        struct netaddr net;
        char upath[108];
    }addr;
}sock_t;


void sock_tostring(sock_t *self);
int sock_init(sock_t *self, const char *info);
inline static void sock_term(sock_t *self)
{
    safe_close(self->fd);
    memset(self, 0, sizeof(sock_t));
}

int sock_accept(sock_t *self, sock_t *cli);
int sock_write(sock_t *self, void *data, size_t len);
int sock_read(sock_t *self, void *data, size_t size);

inline static int sock_fd(sock_t *self)
{
    return self->fd;
}

inline static int sock_is_closed(sock_t *self)
{
    return (sock_fd(self) < 0) ? 1 : 0;
}

inline static void sock_close(sock_t *self)
{
    safe_close(self->fd);
}

inline static int sock_set_nonblock(sock_t *self)
{
    return set_nonblock(sock_fd(self));
}

inline static void sock_lcopy(sock_t *src, sock_t *dst)
{
    if(src && dst)
        memcpy(dst, src, sizeof(sock_t));
}

/**
 *Desc: new a sock_t object.
 *Argument:
 *      @info: input, format is: {'@'|'>'}{"tcp"|"udp"|"unix"}{ip|unix_path}[:port]
 *          for example:
 *          info = "@tcp:192.168.222.254:8000", 
 *          info = ">unix:/tmp/NasEvnSrv",
 *          info = "@udp:127.0.0.1:8000",
 *
 *Return: sock_t *
 */
inline static sock_t *sock_new(const char *info)
{
    sock_t *self = (sock_t *)MALLOC(sizeof(sock_t));
    if(self){
         if(sock_init(self, info) < 0){
            DBG("sock_init fail");
            safe_free(self);
         }
    }

    return self;
}

inline static void sock_destroy(sock_t **p_self)
{
    if(p_self && *p_self){
        sock_term(*p_self);
        safe_free(*p_self);
    }
}


#ifdef __cplusplus
}
#endif


#endif // CLIBS_SOCK_H_
