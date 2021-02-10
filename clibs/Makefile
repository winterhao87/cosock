MODULE := lua_f_epoll.so
MODULE += lua_f_sock.so

.PHONY : clean all install

CFLAGS += -Wall -g -fPIC -DDEBUG
LUA_INC = /usr/local/include/luajit-2.0
# LDFLAGS += -llua
LDFLAGS += -lluajit-5.1

INSTALL= install -p
INSTALL_SO= $(INSTALL) -m 0644
LUA_CLIB_PATH=/usr/local/lib/lua/5.1

all: ${MODULE}
	@echo "compile ${MODULE} done!"

install:
	$(INSTALL_SO) *.so $(LUA_CLIB_PATH)

%.so : %.o
	@$(CC) --shared -o $@ $^ $(LDFLAGS)
	@mv $@ $(subst lua_f_,,$@)

%.o : %.c
	$(CC) $(CFLAGS) -c $^ -I${LUA_INC}

clean:
	$(RM) *.o $(MODULE) *.so