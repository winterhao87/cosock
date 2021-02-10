#include "util.h"

int set_nonblock(int fd) {
    int val;
    if((val = fcntl(fd, F_GETFL, 0)) == -1){
        perror("fcntl get");
        return -1;
    }

	if (val & O_NONBLOCK)
		return 0;

    val |= O_NONBLOCK;
    if(fcntl(fd, F_SETFL, val) == -1){
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

    if(size == 0)
        return 0;

    ssize_t ret = -1;
_AGAIN:
    ret = read(fd, data, size);
    if(ret < 0){
        if(errno == EINTR)
            goto _AGAIN;

        if(errno == EAGAIN || errno == EWOULDBLOCK){
            DBG("EAGAIN");
            return 0;
        }

        return -1;
    }

    return ret;
}

int Write(int fd, void *data, size_t len) {
    if(fd < 0 || data == NULL){
        DBG("invalid args");
        return -1;
    }

    if(len == 0)
        return 0;

    ssize_t ret = -1;
_AGAIN:
    ret = write(fd, data, len);
    if(ret < 0){
        if(errno == EINTR)
            goto _AGAIN;

        if(errno == EAGAIN || errno == EWOULDBLOCK){
            DBG("EAGAIN");
            return 0;
        }

        return -1;
    }

    return ret;
}

int8_t is_ipv6(const char *ip) {
    size_t len = strlen(ip_str);
    size_t i = 0;
    for (i = 0; i < len; i++) {
        if (ip_str[i] == ':') return 1;
    }

    if (ip[0] == '*') return 1;
    return 0;
}

int tcp_server_create(const char *ip_str, uint16_t port, int backlog)
{
    int domain = AF_INET;
    if (is_ipv6(ip_str)) domain = AF_INET6;

    int fd = socket(domain, SOCK_STREAM, 0);
    if(fd < 0){
        ERR("socket fail");
        goto _FAILE;
    }
    if (set_nonblock(fd) < 0) goto _FAILE;

    int reuseaddr = 1;
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &reuseaddr, sizeof(reuseaddr)) < 0) {
        ERR("setsockopt SO_REUSEADDR fail");
        goto _FAILE;
    }

    int reuseport = 1;
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEPORT, &reuseport, sizeof(reuseport)) < 0) {
        ERR("setsockopt SO_RESUEPORT fail");
        goto _FAILE;
    }

    if (domain == AF_INET) {
        struct sockaddr_in addr;
        memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        if(ip_str[0] == '*')
            addr.sin_addr.s_addr = htonl(INADDR_ANY);
        else
            inet_pton(AF_INET, ip_str, &addr.sin_addr);
            // addr.sin_addr.s_addr = inet_addr(ip_str);

        if(bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0){
            ERR("bind fail");
            goto _FAILE;
        }
    } else {
        struct sockaddr_in6 addr;
        addr.sin6_family = AF_INET6;
        addr.sin6_port = htons(port);
        if (ip_str[0] == '*')
            addr.sin6_addr = in6addr_any;
        else
            inet_pton(AF_INET6, ip_str, &addr.sin6_addr);

        if(bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0){
            ERR("bind fail");
            goto _FAILE;
        }
    }

    if(!backlog)
        backlog = 1000;
    if(listen(fd, backlog) < 0){
        ERR("listen fail");
        goto _FAILE;
    }

    return fd;
    
_FAILE:
    close(fd);
    return -1;
}

int Accept(int sock, struct sockaddr *cli_addr, socklen_t *addrlen, int flags)
{
    return accept4(sock, cli_addr, addr_len, flags);
}

int tcp_client_create(const char *ip_str, uint16_t port, int8_t *is_connected)
{
    int domain = AF_INET;
    if (is_ipv6(ip_str)) domain = AF_INET6;
    if (is_connected) *is_connected = 0;

    int fd = socket(domain, SOCK_STREAM, 0);
    if(fd < 0) return -1;
    if (set_nonblock(fd) < 0) goto _FAILE;

    if (domain == AF_INET) {
        struct sockaddr_in addr;
        memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        addr.sin_addr.s_addr = inet_addr(ip_str);

        if(connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
            goto _FAILE;
    } else {
        struct sockaddr_in6 addr;
        addr.sin6_family = AF_INET6;
        addr.sin6_port = htons(port);
        inet_pton(AF_INET6, ip_str, &addr.sin6_addr);

        if(connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
            goto _FAILE;
    }

    if (is_connected) *is_connected = 1;
    return fd;

_FAILE:
    if (errno == EINPROGRESS) return fd;

    close(fd);
    return -1;
}

int udp_server_create(const char *ip_str, uint16_t port)
{
    int domain = AF_INET;
    if (is_ipv6(ip_str)) domain = AF_INET6;

    int fd = socket(domain, SOCK_DGRAM, 0);
    if(fd < 0){
        ERR("socket fail");
        return -1;
    }
    if (set_nonblock(fd) < 0) goto _FAILE;

    int reuseaddr = 1;
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &reuseaddr, sizeof(reuseaddr)) < 0) {
        ERR("setsockopt SO_REUSEADDR fail");
        goto _FAILE;
    }

    int reuseport = 1;
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEPORT, &reuseport, sizeof(reuseport)) < 0) {
        ERR("setsockopt SO_RESUEPORT fail");
        goto _FAILE;
    }

    if (domain == AF_INET) {
        struct sockaddr_in addr;
        memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        if(ip_str[0] == '*')
            addr.sin_addr.s_addr = htonl(INADDR_ANY);
        else
            addr.sin_addr.s_addr = inet_addr(ip_str);

        if(bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0){
            ERR("bind fail");
            goto _FAILE;
        }
    } else {
        struct sockaddr_in6 addr;
        addr.sin6_family = AF_INET6;
        addr.sin6_port = htons(port);
        if (ip_str[0] == '*')
            addr.sin6_addr = in6addr_any;
        else
            inet_pton(AF_INET6, ip_str, &addr.sin6_addr);

        if(bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0){
            ERR("bind fail");
            goto _FAILE;
        }
    }

    return fd;
    
_FAILE:
    close(fd);
    return -1;
}

int udp_client_create(const char *ip_str, uint16_t port)
{
    int domain = AF_INET;
    if (is_ipv6(ip_str)) domain = AF_INET6;

    int fd = socket(domain, SOCK_DGRAM, 0);
    if(fd < 0) return -1;
    if (set_nonblock(fd) < 0) goto _FAILE;

    if (domain == AF_INET) {
        struct sockaddr_in addr;
        memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        addr.sin_addr.s_addr = inet_addr(ip_str);

        if(connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0){
            ERR("connect fail");
            goto _FAILE;
        }
    } else {
        struct sockaddr_in6 addr;
        addr.sin6_family = AF_INET6;
        addr.sin6_port = htons(port);
        inet_pton(AF_INET6, ip_str, &addr.sin6_addr);

        if(connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
            goto _FAILE;
    }

    return fd;
    
_FAILE:
    close(fd);
    return -1;
}

int unix_tcp_server_create(const char *path, int backlog)
{
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if(fd < 0) return -1;

    unlink(path);
    
    struct sockaddr_un addr;
    socklen_t addr_len;
    memset(&addr, 0, sizeof(addr));
    strcpy(addr.sun_path, path);
    addr.sun_family = AF_UNIX;
    addr_len = sizeof(addr.sun_family) + strlen(addr.sun_path);
    if(bind(fd, (struct sockaddr *)&addr, addr_len) < 0)
        goto _FAIL;
    
    if(!backlog)
        backlog = 1000;
    if(listen(fd, backlog) < 0)
        goto _FAIL;

    return fd;

_FAIL:
    close(fd);
    return -1;
}

int unix_tcp_client_create(const char *path)
{
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if(fd < 0) return -1;

    struct sockaddr_un addr;
    socklen_t addr_len;
    memset(&addr, 0, sizeof(addr));
    strcpy(addr.sun_path, path);
    addr.sun_family = AF_UNIX;
    addr_len = sizeof(addr.sun_family) + strlen(addr.sun_path);
    if(connect(fd, (struct sockaddr *)&addr, addr_len) < 0){
        close(fd);
        return -1;
    }   

    return fd;
}

int unix_udp_server_create(const char *path)
{
    unlink(path);

    int sock = socket(AF_UNIX, SOCK_DGRAM, 0);
    if (sock < 0) return -1;

    struct sockaddr_un addr;
    socklen_t addr_len;
    strcpy(addr.sun_path, path);
    addr.sun_family = AF_UNIX;
    addr_len = sizeof(addr.sun_family) + strlen(addr.sun_path);
    if(bind(sock, (struct sockaddr *)&addr, addr_len) < 0)
        goto _FAIL;

    return sock;

_FAIL:
    safe_close(sock);
    return -1;
}

int unix_udp_client_create(const char *path)
{
    int sock = socket(AF_UNIX, SOCK_DGRAM, 0);
    if (sock < 0) return -1;

    struct sockaddr_un addr;
    socklen_t addr_len;
    strcpy(addr.sun_path, path);
    addr.sun_family = AF_UNIX;
    addr_len = sizeof(addr.sun_family) + strlen(addr.sun_path);
    if (connect(sock, (struct sockaddr *)&addr, addr_len) < 0)
        safe_close(sock);

    return sock;
}
