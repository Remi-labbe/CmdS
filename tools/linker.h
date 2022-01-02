#ifndef LINKER__H
#define LINKER__H

#include <stdbool.h>
#include <sys/types.h>
#include "config.h"

#ifndef WD_LEN
#define WD_LEN 512
#endif

#ifndef REQ_BUF_SIZE
#define REQ_BUF_SIZE 2048
#endif

typedef struct client {
  pid_t pid;
  char working_dir[WD_LEN];
} client;

typedef struct linker linker;

/*
 * Create and initialise a new linker
 * Connect to it
 */
extern linker *linker_init(const char *name);

/*
 * Connect to an existing linker
 */
extern linker *linker_connect(const char *name);

/*
 * Add the client c to the linker queue
 */
extern int linker_push(linker *lin, const client *c);

/*
 * Get the first client in the queue, put it in buf
 */
extern int linker_pop(linker *lin, client *buf);
/*
 * Free all resources allocated to the linker
 */
extern void linker_dispose(linker **linker_p);

#endif
