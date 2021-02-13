.PHONY : clean all install

# CFLAGS += -Wall -g -fPIC -DDEBUG
CFLAGS += -Wall -g -fPIC
LUA_INC = /usr/local/include/luajit-2.0
# LDFLAGS += -llua
LDFLAGS += -lluajit-5.1

INSTALL= install -p
INSTALL_SO= $(INSTALL) -m 0644
LUA_CLIB_PATH=/usr/local/lib/lua/5.1

all: libepoll.so libsock.so
	@echo "done"

libepoll.so : lua_f_epoll.o epoll.o util.o
	@$(CC) --shared -o $@ $^ $(LDFLAGS)

libsock.so : lua_f_sock.o sock.o util.o
	@$(CC) --shared -o $@ $^ $(LDFLAGS)

%.o : %.c
	$(CC) $(CFLAGS) -c $^ -I${LUA_INC}

install: libepoll.so libsock.so
	$(INSTALL_SO) libepoll.so ${LUA_CLIB_PATH}/epoll.so
	$(INSTALL_SO) libsock.so ${LUA_CLIB_PATH}/sock.so
	cp -rf ../libs/cosock.lua ${LUA_CLIB_PATH}/

clean:
	$(RM) *.o $(MODULE) *.so