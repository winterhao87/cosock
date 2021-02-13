#include "../clibs/epoll.h"
#include "../clibs/sock.h"

static void do_cli(epoll_fd_t *r, sock_t *cli, struct epoll_event *ev) {
    if (!cli->is_connected) {
        int err = 0;
        socklen_t len = sizeof(err);
        if (getsockopt(sock_fd(cli), SOL_SOCKET, SO_ERROR, (void *)&err, &len) == -1 || err) {
            DBG("cli getsockopt fail: err=%d", err);
            goto _FAILE;
        }

        printf("cli fd=%d connect succ\n", sock_fd(cli));
        cli->is_connected = 1;
    }
    DBG("cli fd=%d, event=%d", sock_fd(cli), ev->events);

    static int index = 0;
    if (ev->events & EPOLLOUT) {
        char buf[1024];
        snprintf(buf, sizeof(buf), "hello %dth", index);
        index++;
        if (sock_write(cli, buf, strlen(buf) + 1) <= 0) {
            ERR("cli sock_write fail");
            goto _FAILE;
        }

        DBG("cli sock_write ok");
        epoll_fd_mod(r, sock_fd(cli), EPOLLIN | EPOLLERR | EPOLLET, cli);
        return;
    }

    if (ev->events & EPOLLIN) {
        char buf[1024];
        int ret = sock_read(cli, buf, sizeof(buf));
        if (ret < 0) {
            ERR("cli sock_read fail");
            goto _FAILE;
        }

        if (ret == 0) {
            DBG("cli closed");
            goto _FAILE;
        }

        buf[ret] = '\0';
        printf("cli sock_read: %s\n", buf);

        if (index <= 10) {
            epoll_fd_mod(r, sock_fd(cli), EPOLLOUT | EPOLLET | EPOLLERR, cli);
        } else {
            goto _FAILE;
        }
        return;
    }

    ERR("unknow events: %d", ev->events);

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

    sock_t *cli = sock_new(">tcp:127.0.0.1:8000");
    assert(cli);
    DBG("cli fd=%d, is_connected=%d", sock_fd(cli), cli->is_connected);

    if (epoll_fd_add(&reactor, sock_fd(cli), EPOLLOUT | EPOLLET | EPOLLERR, cli) < 0) {
        ERR("epoll_fd_add srv fail");
        goto _FAILE;
    }

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
            if (events[i].data.ptr == cli) {
                do_cli(&reactor, cli, &events[i]);
            } else {
                assert(0);
            }
        }
    }

_FAILE:
    epoll_fd_close(&reactor);
    if (cli) sock_destroy(&cli);
    return 0;
}