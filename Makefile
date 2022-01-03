tools_dir=tools/
doc_dir=doc/

CC = gcc

CFLAGS = -std=c18 -Wpedantic -Wall -Wextra -Wconversion -Wwrite-strings \
				 -Werror -fstack-protector-all -fpie -D_XOPEN_SOURCE -O2 -g -I$(tools_dir)

LDFLAGS = -lrt -pthread -Wl,-z,relro,-z,now -pie

VPATH = $(tools_dir)

OBJS = $(tools_dir)linker.o

EXECS = cmdc cmds

DOCS = $(doc_dir)Manuel_Technique.pdf $(doc_dir)Manuel_Utilisateur.pdf

all: $(EXECS)

linker.o: linker.h config.h linker.c

cmdc: config.h client.c $(tools_dir)linker.o
	$(CC) $(LDFLAGS) $^ -o $@

cmds: config.h server.c $(tools_dir)linker.o
	$(CC) $(LDFLAGS) $^ -o $@

$(doc_dir)Manuel_Technique.pdf:
	pandoc --pdf-engine=pdflatex -o $@ $(doc_dir)Manuel_Technique.md

$(doc_dir)Manuel_Utilisateur.pdf:
	pandoc --pdf-engine=pdflatex -o $@ $(doc_dir)Manuel_Utilisateur.md

doc: $(DOCS)

clean:
	$(RM) $(EXECS) $(OBJS) $(DOCS)

tar:
	$(MAKE) clean
	tar -zcf "$(CURDIR).tar.gz" client.c tools/* server.c Makefile
