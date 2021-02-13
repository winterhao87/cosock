#include "sock.h"

#include <sys/socket.h>

static const char *sock_type_str(sock_t *self) {
    switch (self->type) {
        case SOCK_TCP_SERVER:
            return "tcp-serv";
        case SOCK_TCP_CLIENT:
            return "tcp-cli";
        case SOCK_UDP_SERVER:
            return "udp-serv";
        case SOCK_UDP_CLIENT:
            return "udp-cli";
        case SOCK_UNIX_SERVER:
            return "unix-serv";
        case SOCK_UNIX_CLIENT:
            return "unix-cli";
        default:
            return "unknow";
    }
}

void sock_tostring(sock_t *self) {
    printf("sock info: fd(%d), type(%s), ", self->fd, sock_type_str(self));
    sock_type_t type = self->type & SOCKET_TYPE_MASK;
    if (type == SOCK_TCP || type == SOCK_UDP) {
        printf("ip(%s), port(%u)\n", self->addr.net.ip, self->addr.net.port);
    } else if (type == SOCK_UNIX) {
        printf("path(%s)\n", self->addr.upath);
    } else {
        printf("invalid \n");
    }
}
/*
 * for example:
 * serv: @tcp:127.0.0.1:8000
 * client: >tcp:127.0.0.1:8000
 */
static int _parse_socket_info(const char *info, sock_t *self) {
    uint16_t offset = 0;
    uint8_t is_server = 0;

    // parse is_server
    if (info[offset] == '@')
        is_server = 1;
    else if (info[offset] == '>')
        is_server = 0;
    else
        return -1;

    offset += 1;

    // parse domain
    if (strncmp(info + offset, "tcp", 3) == 0) {
        self->type = SOCK_TCP;
        offset += 3;
    } else if (strncmp(info + offset, "udp", 3) == 0) {
        self->type = SOCK_UDP;
        offset += 3;
    } else if (strncmp(info + offset, "unix", 4) == 0) {
        self->type = SOCK_UNIX;
        offset += 4;
    } else
        return -1;

    if (is_server)
        self->type += 1;
    else
        self->type += 2;

    offset += 1;  // skip ":"
    if (self->type > SOCK_UNIX) {
        strncpy(self->addr.upath, info + offset, sizeof(self->addr.upath));
        return 0;
    }

    // parse ip
    char *flag = strstr(info + offset, "[");
    if (flag != NULL) {
        char *end = strstr(flag, "]");
        if (end == NULL) {
            DBG("invalid ipv6");
            return -1;
        }

        uint8_t len = end - flag - 1;
        if (len > (sizeof(self->addr.net.ip) - 1)) return -1;
        memcpy(self->addr.net.ip, flag + 1, len);
        self->addr.net.ip[len] = 0;
        offset += (len + 1 + 2);

        self->domain = AF_INET6;
    } else {
        flag = strstr(info + offset, ":");
        if (flag == NULL) return -1;

        uint8_t len = flag - info - offset;
        if (len > (sizeof(self->addr.net.ip) - 1)) return -1;
        memcpy(self->addr.net.ip, info + offset, len);
        self->addr.net.ip[len] = 0;
        offset += (len + 1);

        if (self->addr.net.ip[0] == '*')
            self->domain = AF_INET6;
        else
            self->domain = AF_INET;
    }

    // parse port
    self->addr.net.port = atoi(info + offset);
    return 0;
}

int sock_init(sock_t *self, const char *info) {
    self->fd = -1;
    DBG("info:%s\n", info);
    if (_parse_socket_info(info, self) < 0) {
        DBG("_parse_socket_info fail");
        return -1;
    }

    switch (self->type) {
        case SOCK_TCP_SERVER:
            self->fd = tcp_server_create(self->addr.net.ip, self->addr.net.port, 1024);
            break;

        case SOCK_TCP_CLIENT:
            self->fd = tcp_client_create(self->addr.net.ip, self->addr.net.port, &self->is_connected);
            break;

        case SOCK_UDP_SERVER:
            self->fd = udp_server_create(self->addr.net.ip, self->addr.net.port);
            break;

        case SOCK_UDP_CLIENT:
            self->fd = udp_client_create(self->addr.net.ip, self->addr.net.port);
            break;

        case SOCK_UNIX_SERVER:
            self->fd = unix_udp_server_create(self->addr.upath);
            break;

        case SOCK_UNIX_CLIENT:
            self->fd = unix_udp_client_create(self->addr.upath);
            break;

        default:
            // should not be here
            DBG("invalid sock type(%u)", self->type);
            assert(0);
    }

    if (self->fd < 0) {
        sock_tostring(self);
        return -1;
    }

    return 0;
}

int sock_accept(sock_t *self, sock_t *cli) {
    assert(self);
    assert(self->type == SOCK_TCP_SERVER);
    assert(cli);

    int fd = -1;
    socklen_t addr_len = 0;
    struct sockaddr_in v4;
    struct sockaddr_in6 v6;

_AGAIN:
    if (self->domain == AF_INET) {
        addr_len = sizeof(v4);
        fd = accept(sock_fd(self), (struct sockaddr *)(&v4), &addr_len);
    } else {
        addr_len = sizeof(v6);
        fd = accept(sock_fd(self), (struct sockaddr *)(&v6), &addr_len);
    }

    if (fd < 0) {
        if (errno == EINTR) goto _AGAIN;
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return -EAGAIN;
        }

        ERR("accept fail");
        return -1;
    }

    if (set_nonblock(fd) < 0) {
        ERR("set_nonblock fail");
        close(fd);
        return -1;
    }

    cli->fd = fd;
    cli->domain = self->domain;
    cli->type = SOCK_TCP_CLIENT;

    if (cli->domain == AF_INET) {
        inet_ntop(AF_INET, &v4.sin_addr, cli->addr.net.ip, addr_len);
        cli->addr.net.port = ntohs(v4.sin_port);
    } else {
        inet_ntop(AF_INET6, &v6.sin6_addr, cli->addr.net.ip, addr_len);
        cli->addr.net.port = ntohs(v6.sin6_port);
    }

    return 0;
}

int sock_write(sock_t *self, void *data, size_t len) {
    assert(self);
    assert(data);
    assert(self->type != SOCK_UNKONW_TYPE);

    if (sock_is_closed(self)) {
        DBG("socket closed");
        return -1;
    }

    int ret = Write(sock_fd(self), data, len);
    if (ret < 0) {
        ERR("Write fail");
        return -1;
    }

    return ret;
}

int sock_read(sock_t *self, void *data, size_t size) {
    assert(self);
    assert(data);
    assert(self->type != SOCK_UNKONW_TYPE);

    if (sock_is_closed(self) < 0) {
        DBG("socket closed");
        return -1;
    }

    int ret = Read(sock_fd(self), data, size);
    if (ret < 0 && ret != -EAGAIN) {
        ERR("fd_read fail");
        return -1;
    }

    return ret;
}

int8_t is_ipv6(const char *ip) {
    size_t len = strlen(ip);
    size_t i = 0;
    for (i = 0; i < len; i++) {
        if (ip[i] == ':') return 1;
    }

    if (ip[0] == '*') return 1;
    return 0;
}

int tcp_server_create(const char *ip_str, uint16_t port, int backlog) {
    int domain = AF_INET;
    if (is_ipv6(ip_str)) domain = AF_INET6;

    int fd = socket(domain, SOCK_STREAM, 0);
    if (fd < 0) {
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
        if (ip_str[0] == '*')
            addr.sin_addr.s_addr = htonl(INADDR_ANY);
        else
            inet_pton(AF_INET, ip_str, &addr.sin_addr);
        // addr.sin_addr.s_addr = inet_addr(ip_str);

        if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
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

        if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
            ERR("bind fail");
            goto _FAILE;
        }
    }

    if (!backlog) backlog = 1000;
    if (listen(fd, backlog) < 0) {
        ERR("listen fail");
        goto _FAILE;
    }

    return fd;

_FAILE:
    close(fd);
    return -1;
}

int tcp_client_create(const char *ip_str, uint16_t port, int8_t *is_connected) {
    int domain = AF_INET;
    if (is_ipv6(ip_str)) domain = AF_INET6;
    if (is_connected) *is_connected = 0;

    int fd = socket(domain, SOCK_STREAM, 0);
    if (fd < 0) return -1;
    if (set_nonblock(fd) < 0) goto _FAILE;

    if (domain == AF_INET) {
        struct sockaddr_in addr;
        memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        addr.sin_addr.s_addr = inet_addr(ip_str);

        if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) goto _FAILE;
    } else {
        struct sockaddr_in6 addr;
        addr.sin6_family = AF_INET6;
        addr.sin6_port = htons(port);
        inet_pton(AF_INET6, ip_str, &addr.sin6_addr);

        if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) goto _FAILE;
    }

    if (is_connected) *is_connected = 1;
    return fd;

_FAILE:
    if (errno == EINPROGRESS) return fd;

    close(fd);
    return -1;
}

int udp_server_create(const char *ip_str, uint16_t port) {
    int domain = AF_INET;
    if (is_ipv6(ip_str)) domain = AF_INET6;

    int fd = socket(domain, SOCK_DGRAM, 0);
    if (fd < 0) {
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
        if (ip_str[0] == '*')
            addr.sin_addr.s_addr = htonl(INADDR_ANY);
        else
            addr.sin_addr.s_addr = inet_addr(ip_str);

        if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
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

        if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
            ERR("bind fail");
            goto _FAILE;
        }
    }

    return fd;

_FAILE:
    close(fd);
    return -1;
}

int udp_client_create(const char *ip_str, uint16_t port) {
    int domain = AF_INET;
    if (is_ipv6(ip_str)) domain = AF_INET6;

    int fd = socket(domain, SOCK_DGRAM, 0);
    if (fd < 0) return -1;
    if (set_nonblock(fd) < 0) goto _FAILE;

    if (domain == AF_INET) {
        struct sockaddr_in addr;
        memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        addr.sin_addr.s_addr = inet_addr(ip_str);

        if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
            ERR("connect fail");
            goto _FAILE;
        }
    } else {
        struct sockaddr_in6 addr;
        addr.sin6_family = AF_INET6;
        addr.sin6_port = htons(port);
        inet_pton(AF_INET6, ip_str, &addr.sin6_addr);

        if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) goto _FAILE;
    }

    return fd;

_FAILE:
    close(fd);
    return -1;
}

int unix_tcp_server_create(const char *path, int backlog) {
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) return -1;

    unlink(path);

    struct sockaddr_un addr;
    socklen_t addr_len;
    memset(&addr, 0, sizeof(addr));
    strcpy(addr.sun_path, path);
    addr.sun_family = AF_UNIX;
    addr_len = sizeof(addr.sun_family) + strlen(addr.sun_path);
    if (bind(fd, (struct sockaddr *)&addr, addr_len) < 0) goto _FAIL;

    if (!backlog) backlog = 1000;
    if (listen(fd, backlog) < 0) goto _FAIL;

    return fd;

_FAIL:
    close(fd);
    return -1;
}

int unix_tcp_client_create(const char *path) {
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) return -1;

    struct sockaddr_un addr;
    socklen_t addr_len;
    memset(&addr, 0, sizeof(addr));
    strcpy(addr.sun_path, path);
    addr.sun_family = AF_UNIX;
    addr_len = sizeof(addr.sun_family) + strlen(addr.sun_path);
    if (connect(fd, (struct sockaddr *)&addr, addr_len) < 0) {
        close(fd);
        return -1;
    }

    return fd;
}

int unix_udp_server_create(const char *path) {
    unlink(path);

    int sock = socket(AF_UNIX, SOCK_DGRAM, 0);
    if (sock < 0) return -1;

    struct sockaddr_un addr;
    socklen_t addr_len;
    strcpy(addr.sun_path, path);
    addr.sun_family = AF_UNIX;
    addr_len = sizeof(addr.sun_family) + strlen(addr.sun_path);
    if (bind(sock, (struct sockaddr *)&addr, addr_len) < 0) goto _FAIL;

    return sock;

_FAIL:
    safe_close(sock);
    return -1;
}

int unix_udp_client_create(const char *path) {
    int sock = socket(AF_UNIX, SOCK_DGRAM, 0);
    if (sock < 0) return -1;

    struct sockaddr_un addr;
    socklen_t addr_len;
    strcpy(addr.sun_path, path);
    addr.sun_family = AF_UNIX;
    addr_len = sizeof(addr.sun_family) + strlen(addr.sun_path);
    if (connect(sock, (struct sockaddr *)&addr, addr_len) < 0) safe_close(sock);

    return sock;
}