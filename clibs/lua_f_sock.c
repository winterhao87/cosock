#include "lua_f_util.h"
#include "sock.h"

#define SOCK_METATABLE_NAME "ywh.SockMT"

#define check_sock(L) (sock_t *)luaL_checkudata(L, 1, SOCK_METATABLE_NAME)

static int lua_f_sock_close(lua_State *L) {
    sock_t *self = check_sock(L);
    sock_term(self);
    return 0;
}

static int lua_f_sock_fd(lua_State *L) {
    sock_t *self = check_sock(L);
    lua_pushinteger(L, sock_fd(self));
    return 1;
}

static int lua_f_sock_set_nonblock(lua_State *L) {
    sock_t *self = check_sock(L);
    if (sock_set_nonblock(self) < 0) {
        RETERR("sock_set_nonblock fail");
    }

    lua_pushboolean(L, 1);
    return 1;
}

static int lua_f_sock_accept(lua_State *L) {
    sock_t *self = check_sock(L);
    sock_t cli;

    int ret = sock_accept(self, &cli);
    if (ret == -EAGAIN) {
        lua_pushnil(L);
        return 1;
    }
    if (ret < 0) {
        RETERR("sock_accept fail");
    }

    sock_t *udata = lua_newuserdata(L, sizeof(sock_t));
    sock_lcopy(&cli, udata);

    luaL_getmetatable(L, SOCK_METATABLE_NAME);
    lua_setmetatable(L, -2);
    return 1;
}

static int lua_f_sock_write(lua_State *L) {
    if (lua_gettop(L) < 2) {
        RETERR("invalid args");
    }

    sock_t *self = check_sock(L);
    size_t len = 0;
    const char *data = luaL_checklstring(L, 2, &len);

    if (sock_is_closed(self)) {
        RETERR("socket has closed");
    }

    if (0 == len) {
        lua_pushinteger(L, 0);
        return 1;
    }

    int ret = sock_write(self, (void *)data, len);
    if (ret < 0) {
        RETERR("sock_send fail");
    }

    lua_pushinteger(L, ret);
    return 1;
}

static int lua_f_sock_read(lua_State *L) {
    if (lua_gettop(L) < 1) {
        RETERR("invalid args");
    }

    sock_t *self = check_sock(L);
    if (sock_is_closed(self)) {
        RETERR("socket has closed");
    }

    char buf[4096];
    int ret = sock_read(self, buf, sizeof(buf));
    if (ret < 0 && ret != -EAGAIN) {
        DBG("err: sock_read fail");
        lua_pushinteger(L, 0);
        lua_pushnil(L);
        lua_pushfstring(L, "%s: sock_read fail", strerror(errno));
        return 3;
    }

    lua_pushinteger(L, ret);
    if (ret <= 0) {
        return 1;
    }

    lua_pushlstring(L, buf, ret);
    return 2;
}

static int lua_f_sock_is_closed(lua_State *L) {
    sock_t *self = check_sock(L);
    lua_pushboolean(L, sock_is_closed(self));
    return 1;
}

static const struct luaL_Reg lua_f_sock_func[] = {
    {"close", lua_f_sock_close},
    {"fd", lua_f_sock_fd},
    {"set_nonblock", lua_f_sock_set_nonblock},
    {"accept", lua_f_sock_accept},
    {"write", lua_f_sock_write},
    {"read", lua_f_sock_read},
    {"is_closed", lua_f_sock_is_closed},
    {NULL, NULL},
};

static int lua_f_sock_create(lua_State *L) {
    const char *info = luaL_checkstring(L, 1);
    sock_t *self = lua_newuserdata(L, sizeof(sock_t));
    if (sock_init(self, info) < 0) {
        RETERR("sock_init fail");
    }

    luaL_getmetatable(L, SOCK_METATABLE_NAME);
    lua_setmetatable(L, -2);
    return 1;
}

static int lua_f_sock_version(lua_State *L) {
    const char *ver = "Lua-Sock V0.0.1 by wenhaoye@126.com";
    lua_pushstring(L, ver);
    return 1;
}

static const struct luaL_Reg lua_f_sock_mod[] = {
    {"new", lua_f_sock_create},
    {"version", lua_f_sock_version},
    {NULL, NULL},
};

int luaopen_sock(lua_State *L) {
    luaL_newmetatable(L, SOCK_METATABLE_NAME);
    LTABLE_ADD_CFUNC(L, -1, "__gc", lua_f_sock_close);
    lua_newtable(L);
    luaL_register(L, NULL, lua_f_sock_func);
    lua_setfield(L, -2, "__index");
    lua_pop(L, 1);

    luaL_register(L, "sock", lua_f_sock_mod);
    return 1;
}
