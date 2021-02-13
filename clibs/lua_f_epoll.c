#include "epoll.h"
#include "lua_f_util.h"

#define EPOLL_METATABLE_NAME "ywh.EpollMT"

#define check_epoll_fd(L) luaL_checkudata(L, 1, EPOLL_METATABLE_NAME)

static int lua_f_epoll_close(lua_State *L) {
    epoll_fd_t *self = (epoll_fd_t *)check_epoll_fd(L);
    epoll_fd_close(self);
    return 0;
}

static int lua_f_epoll_add(lua_State *L) {
    if (lua_gettop(L) < 3) {
        RETERR("invalid args");
    }

    epoll_fd_t *self = (epoll_fd_t *)check_epoll_fd(L);
    int fd = luaL_checkint(L, 2);
    int event = luaL_checkint(L, 3);
    void *ptr = NULL;

    if (lua_isuserdata(L, 4)) ptr = lua_touserdata(L, 4);

    if (fd < 0) {
        RETERR("invalid fd");
    }

    if (epoll_fd_add(self, fd, event, ptr) < 0) {
        RETERR("epoll_fd_add fail");
    }

    lua_pushboolean(L, 1);
    return 1;
}

static int lua_f_epoll_modify(lua_State *L) {
    if (lua_gettop(L) < 3) {
        RETERR("invalid args");
    }

    epoll_fd_t *self = (epoll_fd_t *)check_epoll_fd(L);
    int fd = luaL_checkint(L, 2);
    int event = luaL_checkint(L, 3);
    void *ptr = NULL;

    if (lua_isuserdata(L, 4)) ptr = lua_touserdata(L, 4);

    if (fd < 0) {
        RETERR("invalid fd");
    }

    if (epoll_fd_mod(self, fd, event, ptr) < 0) {
        RETERR("epoll_fd_mod fail");
    }

    lua_pushboolean(L, 1);
    return 1;
}

static int lua_f_epoll_del(lua_State *L) {
    if (lua_gettop(L) < 2) {
        RETERR("invalid args");
    }

    epoll_fd_t *self = (epoll_fd_t *)check_epoll_fd(L);
    int fd = luaL_checkint(L, 2);

    if (fd < 0) {
        RETERR("invalid fd");
    }

    if (epoll_fd_del(self, fd) < 0) {
        RETERR("epoll_fd_del fail");
    }

    lua_pushboolean(L, 1);
    return 1;
}

static int lua_f_epoll_wait(lua_State *L) {
    epoll_fd_t *self = (epoll_fd_t *)check_epoll_fd(L);
    int timeout = luaL_optint(L, 2, -1);

    int n = epoll_fd_wait(self, timeout);
    if (n < 0) {
        lua_pushnil(L);
        lua_pushnil(L);
        lua_pushfstring(L, "%s: epoll_fd_wait fail", strerror(errno));
        return 3;
    }
    DBG("epoll_fd_wait timeout=%d, n=%d", timeout, n);

    char key[32];
    memset(key, 0, sizeof(key));

    lua_newtable(L);
    int i;
    for (i = 0; i < n; ++i) {
        lua_pushinteger(L, self->events[i].events);
        lua_rawseti(L, -2, i + 1);
    }

    lua_newtable(L);
    for (i = 0; i < n; ++i) {
        int x = snprintf(key, sizeof(key), "%u", self->events[i].data.fd);
        key[x] = '\0';

        // lua_pushlightuserdata(L, self->events[i].data.ptr);
        lua_pushstring(L, key);
        lua_rawseti(L, -2, i + 1);

        DBG("setfield: %s", key);
    }

    return 2;
}

static const struct luaL_Reg lua_f_epoll_func[] = {
    {"close", lua_f_epoll_close}, {"add", lua_f_epoll_add},   {"modify", lua_f_epoll_modify},
    {"del", lua_f_epoll_del},     {"wait", lua_f_epoll_wait}, {NULL, NULL},
};

static int lua_f_epoll_create(lua_State *L) {
    size_t size = luaL_optint(L, 1, 0);
    epoll_fd_t *self = (epoll_fd_t *)lua_newuserdata(L, sizeof(epoll_fd_t));
    if (epoll_fd_create(self, size) < 0) {
        RETERR("epoll_fd_create fail");
    }

    luaL_getmetatable(L, EPOLL_METATABLE_NAME);
    lua_setmetatable(L, -2);
    return 1;
}

static int lua_f_epoll_version(lua_State *L) {
    const char *ver = "Lua-Epoll V0.0.1 by wenhaoye@126.com";
    lua_pushstring(L, ver);
    return 1;
}

static const struct luaL_Reg lua_f_epoll_mod[] = {
    {"create", lua_f_epoll_create},
    {"version", lua_f_epoll_version},
    {NULL, NULL},
};

/*
  mt = {
    __gc = lua_f_epoll_close,
    __index = {
        close = lua_f_epoll_close,
        add = lua_f_epoll_add,
        modify = lua_f_epoll_modify,
        del = lua_f_epoll_del,
        wait = lua_f_epoll_wait,
    },
  };

  epoll = {
    create = lua_f_epoll_create,
    version = lua_f_epoll_version,
    EPOLLIN = EPOLLIN,
    EPOLLPRI = EPOLLPRI,
    EPOLLOUT = EPOLLOUT,
    EPOLLRDNORM = EPOLLRDNORM,
    EPOLLRDBAND = EPOLLRDBAND,
    EPOLLWRNORM = EPOLLWRNORM,
    EPOLLWRBAND = EPOLLWRBAND,
    EPOLLMSG = EPOLLMSG,
    EPOLLERR = EPOLLERR,
    EPOLLHUP = EPOLLHUP,
    EPOLLRDHUP = EPOLLRDHUP,
    EPOLLONESHOT = EPOLLONESHOT,
    EPOLLET = EPOLLET,
  };

*/
int luaopen_epoll(lua_State *L) {
    luaL_newmetatable(L, EPOLL_METATABLE_NAME);
    LTABLE_ADD_CFUNC(L, -1, "__gc", lua_f_epoll_close);
    lua_newtable(L);
    luaL_register(L, NULL, lua_f_epoll_func);
    lua_setfield(L, -2, "__index");
    lua_pop(L, 1);

    luaL_register(L, "epoll", lua_f_epoll_mod);

#define SETCONST(EVENT)       \
    lua_pushnumber(L, EVENT); \
    lua_setfield(L, -2, #EVENT)

    // push const values into epoll table.
    SETCONST(EPOLLIN);
    SETCONST(EPOLLPRI);
    SETCONST(EPOLLOUT);
    SETCONST(EPOLLRDNORM);
    SETCONST(EPOLLRDBAND);
    SETCONST(EPOLLWRNORM);
    SETCONST(EPOLLWRBAND);
    SETCONST(EPOLLMSG);
    SETCONST(EPOLLERR);
    SETCONST(EPOLLHUP);
    SETCONST(EPOLLRDHUP);
    SETCONST(EPOLLONESHOT);
    SETCONST(EPOLLET);

#undef SETCONST

    return 1;
}
