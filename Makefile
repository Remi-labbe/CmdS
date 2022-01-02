tools_dir=tools/

CC = gcc

CFLAGS = -std=c18 -Wpedantic -Wall -Wextra -Wconversion -Wwrite-strings \
				 -Werror -fstack-protector-all -fpie -D_XOPEN_SOURCE -O2 -g -I$(tools_dir)

LDFLAGS = -lrt -pthread -Wl,-z,relro,-z,now -pie

VPATH = $(tools_dir)

OBJS = $(tools_dir)linker.o

EXECS = cmdc cmds

all: $(EXECS)

linker.o: linker.h config.h linker.c

cmdc: config.h client.c $(tools_dir)linker.o
	$(CC) $(LDFLAGS) $^ -o $@

cmds: config.h server.c $(tools_dir)linker.o
	$(CC) $(LDFLAGS) $^ -o $@

clean:
	$(RM) $(EXECS) $(OBJS)

tar:
	$(MAKE) clean
	tar -zcf "$(CURDIR).tar.gz" client.c tools/* server.c Makefile
