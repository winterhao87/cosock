#ifndef CLIBS_LUA_F_UTIL_H_
#define CLIBS_LUA_F_UTIL_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>

#include "util.h"

#define DSERR()                             \
    do {                                    \
        lua_pushnil(L);                     \
        lua_pushstring(L, strerror(errno)); \
        return 2;                           \
    } while (0)

#define RETERR(_e)                                           \
    do {                                                     \
        DBG("err: %s", (_e));                                \
        lua_pushnil(L);                                      \
        lua_pushfstring(L, "%s: %s", strerror(errno), (_e)); \
        return 2;                                            \
    } while (0)

#define LTABLE_ADD_CFUNC(L, index, name, func) \
    lua_pushcfunction(L, (func));              \
    lua_setfield(L, (index)-1, (name));

static inline void stack_dump(lua_State *L) {
    int i;
    int top = lua_gettop(L);
    printf("Stack has %d elements:\n\t", top);
    for (i = 1; i <= top; ++i) {
        int type = lua_type(L, i);
        switch (type) {
            case LUA_TSTRING:  // strings
                printf("'%s'", lua_tostring(L, i));
                break;

            case LUA_TBOOLEAN:  // booleans
                printf(lua_toboolean(L, i) ? "true" : "false");
                break;

            case LUA_TNUMBER:  // numbers
                // printf("%g", lua_tonumber(L, i));
                printf("%ld", lua_tointeger(L, i));
                break;
            default:  // other values
                printf("%s", lua_typename(L, lua_type(L, i)));
                break;
        }

        printf("\t");
    }
    printf("\n");
}

#ifdef __cplusplus
}
#endif

#endif  // CLIBS_LUA_F_UTIL_H_
