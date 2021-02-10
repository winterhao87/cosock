#include "sock.h"

#include <sys/socket.h>

static const char *sock_type_str(sock_t *self)
{
	switch(self->type) {
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

void sock_tostring(sock_t *self)
{
	printf("sock info: fd(%d), type(%s), ", self->fd, sock_type_str(self));
	sock_type_t type = self->type & IO_SOCKET_TYPE_MASK;
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
static int _parse_socket_info(const char *info, sock_t *self)
{
    uint16_t offset = 0;
    uint8_t is_server = 0;

    // parse is_server
    if(info[offset] == '@')
        is_server = 1;
    else if(info[offset] == '>')
        is_server = 0;
    else
        return -1;

    offset += 1;

    // parse domain
    if(strncmp(info+offset, "tcp", 3) == 0){
        self->type = SOCK_TCP;
        offset += 3;
    }
    else if(strncmp(info+offset, "udp", 3) == 0){
        self->type = SOCK_UDP;
        offset += 3;
    }
    else if(strncmp(info+offset, "unix", 4) == 0){
        self->type = SOCK_UNIX;
        offset += 4;
    }
    else
        return -1;

    if(is_server)
        self->type += 1;
    else
        self->type += 2;

    offset += 1; // skip ":"
    if(self->type > SOCK_UNIX){
        strncpy(self->addr.upath, info+offset, sizeof(self->addr.upath));
        return 0;
    }

    // parse ip
    char *flag = strstr(info+offset, "[");
    if (flag != NULL) {
        char *end = strstr(flag, "]");
        if (end == NULL) {
            DBG("invalid ipv6");
            return -1;
        }

        uint8_t len = end - flag - 1;
        if (len > (sizeof(self->addr.net.ip) - 1)) return -1;
        memcpy(self->addr.net.ip, flag+1, len);
        self->addr.net.ip[len] = 0;
        offset += (len+1+2);

        self->addr.domain = AF_INET6;
    } else {
        flag = strstr(info+offset, ":");
        if(flag == NULL)
            return -1;

        uint8_t len = flag - info - offset;
        if(len > (sizeof(self->addr.net.ip) - 1)) return -1;
        memcpy(self->addr.net.ip, info+offset, len);
        self->addr.net.ip[len] = 0;
        offset += (len+1);

        if (self->addr.net.ip[0] == '*')
            self->addr.domain = AF_INET6;
        else 
            self->addr.domain = AF_INET;
    }

    // parse port
    self->addr.net.port = atoi(info+offset);
    return 0;
}

int sock_init(sock_t *self, const char *info)
{
    self->fd = -1;
	DBG("info:%s\n", info);
    if(_parse_socket_info(info, self) < 0){
        DBG("_parse_socket_info fail");
        return -1;
    }

    switch(self->type){
    case SOCK_TCP_SERVER:
        self->fd = tcp_server_create(self->addr.net.ip, self->addr.net.port, 1024);
        break;
        
    case SOCK_TCP_CLIENT:
        self->fd = tcp_client_create(self->addr.net.ip, self->addr.net.port);
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

    if(self->fd < 0) {
		sock_tostring(self);
        return -1;
	}

    return 0;
}

int sock_accept(sock_t *self, sock_t *cli)
{
    assert(self);
    assert(self->type == SOCK_TCP_SERVER);
    assert(cli);

    int fd = -1;
    socklen_t addr_len = 0;
    struct sockaddr_in v4;
    struct sockaddr_in6 v6;

    if (self->domain == AF_INET) {
        addr_len = sizeof(v4);
        fd = Accept(sock_fd(self), &v4, &addr_len, SOCK_NONBLOCK);
    } else {
        addr_len = sizeof(v6);
        fd = Accept(sock_fd(self), &v6, &addr_len, SOCK_NONBLOCK);
    }

    if(fd < 0) return -1;
    
    cli->fd = fd;
    cli->domain = self->domain;
    cli->type = SOCK_TCP_CLIENT;

    if (cli->domain == AF_INET) {
        inet_ntop(AF_INET, &v4.sin_addr, cli->addr.ip, addr_len);
        cli->addr.port = ntohs(v4.sin_port);
    } else {
        inet_ntop(AF_INET6, &v6.sin6_addr, cli->addr.ip, addr_len);
        cli->addr.port = ntohs(v6.sin6_port);
    }

    return 0;
}

int sock_write(sock_t *self, void *data, size_t len)
{
    assert(self);
    assert(data);
    assert(self->type != SOCK_UNKONW_TYPE);

    if(sock_is_closed(self)){
        DBG("socket closed");
        return -1;
    }

    int ret = Write(sock_fd(self), data, len);
    if(ret < 0){
        ERR("Write fail");
        sock_close(self);
        return -1;
    }

    return ret;
}

int sock_read(sock_t *self, void *data, size_t size)
{
    assert(self);
    assert(data);
    assert(self->type != SOCK_UNKONW_TYPE);

    if(sock_is_closed(self) < 0){
        DBG("socket closed");
        return -1;
    }

    int ret = Read(sock_fd(self), data, size);
    if(ret < 0){
        ERR("fd_read fail");
        sock_close(self);
        return -1;
    }

    return ret;
}
