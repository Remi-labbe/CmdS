linker_dir=linker/

CC = gcc

CFLAGS = -std=c18 -Wpedantic -Wall -Wextra -Wconversion -Wwrite-strings \
				 -Werror -fstack-protector-all -fpie -D_XOPEN_SOURCE -O2 -g -I$(linker_dir)

LDFLAGS = -lrt -pthread -Wl,-z,relro,-z,now -pie

VPATH = $(linker_dir)

OBJS = client.o server.o $(linker_dir)linker.o

EXECS = client server

all: $(EXECS)

linker.o: linker.h linker.c

client: client.c $(linker_dir)linker.o
	$(CC) $(LDFLAGS) $^ -o $@

server: server.c $(linker_dir)linker.o
	$(CC) $(LDFLAGS) $^ -o $@

clean:
	$(RM) $(EXECS) $(OBJS)

tar:
	$(MAKE) clean
	tar -zcf "$(CURDIR).tar.gz" client.c linker/* server.c Makefile
