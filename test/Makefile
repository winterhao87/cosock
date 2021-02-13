all: srv cli

srv: srv.c
	gcc -Wall -g -o $@ $^ -I../clibs -L../clibs -lepoll -lsock

cli: cli.c
	gcc -Wall -g -o $@ $^ -I../clibs -L../clibs -lepoll -lsock

run_srv: srv
	# LD_LIBRARY_PATH=../clibs ./srv
	luajit srv.lua

run_cli: cli 
	# LD_LIBRARY_PATH=../clibs ./cli
	luajit cli.lua

clean:
	rm -rf srv cli 