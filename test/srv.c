#include "../clibs/epoll.h"
#include "../clibs/sock.h"

static void do_srv(epoll_fd_t *r, sock_t *srv, struct epoll_event *ev) {
    sock_t *cli = MALLOC(sizeof(sock_t));
    assert(cli);

    if (sock_accept(srv, cli) < 0) {
        ERR("srv sock_accept fail");
        goto _FAILE;
    }
    printf("srv sock_accept ok: fd=%d, addr=%s:%d\n", sock_fd(cli), cli->addr.net.ip, cli->addr.net.port);

    epoll_fd_add(r, sock_fd(cli), EPOLLIN | EPOLLET | EPOLLERR, cli);
    return;

_FAILE:
    epoll_fd_del(r, sock_fd(srv));
    sock_destroy(&srv);
}

static void handle_cli(epoll_fd_t *r, sock_t *cli, struct epoll_event *ev) {
    DBG("cli ev=%d, fd=%d", ev->events, sock_fd(cli));
    char buf[1024];
    int ret = sock_read(cli, buf, sizeof(buf));
    if (ret < 0) {
        if (ret != -EAGAIN) {
            ERR("cli sock_read fail");
            goto _FAILE;
        }

        DBG("ci sock_read AGAIN");
        return;
    }

    if (ret == 0) {
        DBG("cli closed");
        goto _FAILE;
    }

    buf[ret] = '\0';
    DBG("cli sock_read: %s\n", buf);

    if (sock_write(cli, buf, ret) <= 0) {
        ERR("cli sock_write fail");
        goto _FAILE;
    }
    return;

_FAILE:
    epoll_fd_del(r, sock_fd(cli));
    sock_destroy(&cli);
}

int main(int argc, char **argv) {
    epoll_fd_t reactor;
    if (epoll_fd_create(&reactor, 0) < 0) {
        ERR("epoll_fd_create fail");
        return -1;
    }

    sock_t *srv = sock_new("@tcp:*:8000");
    assert(srv);

    if (epoll_fd_add(&reactor, sock_fd(srv), EPOLLIN | EPOLLET | EPOLLERR, srv) < 0) {
        ERR("epoll_fd_add srv fail");
        goto _FAILE;
    }
    DBG("srv fd==%d", sock_fd(srv));

    while (1) {
        int n = epoll_fd_wait(&reactor, -1);
        if (n <= 0) {
            ERR("epoll_fd_wait fail");
            break;
        }
        DBG("epoll_fd_wait n=%d", n);

        struct epoll_event *events = epoll_events(&reactor);
        int i = 0;
        for (i = 0; i < n; i++) {
            if (events[i].data.ptr == srv) {
                do_srv(&reactor, srv, &events[i]);
            } else {
                handle_cli(&reactor, events[i].data.ptr, &events[i]);
            }
        }
    }

_FAILE:
    epoll_fd_close(&reactor);
    if (srv) sock_destroy(&srv);
    return 0;
}