#ifndef LINKER__H
#define LINKER__H

#include <stdbool.h>
#include <sys/types.h>

/**
* @define WD_LEN max length of the working dir string
*/
#ifndef WD_LEN
#define WD_LEN 512
#endif

/**
* @typedef struct client
*         infos attached to a client
* @field    pid           the client process id
* @field    working_dir   the client working directory
*/
typedef struct client {
  pid_t pid;
  char working_dir[WD_LEN];
} client;

/**
* @typedef linker
*         the synchronised queue structure
* @field    shm_name  the name in which the linker is stored
* @field    head      head index
* @field    tail      tail index
* @field    mutex     mutex shm
* @field    empty     empty blocking shm
* @field    full      full blocking shm
* @field    buffer[]  memory allocated to the linker
*/
typedef struct linker linker;

/**
 * @function  linker_init
 * @abstract  creates a linker
 * @param   name    shm names to store the linker
 */
extern linker *linker_init(const char *name);
/**
 * @function  linker_connect
 * @abstract  connect to an existing linker
 * @param   name    name of the linker's shm
 */
extern linker *linker_connect(const char *name);
/**
 * @function  linker_push
 * @abstract  adds a client to the end of the queue
 * @param   lin   the linker to use
 * @param   c     the client to put in the linker
 */
extern int linker_push(linker *lin, const client *c);
/**
 * @function  linker_pop
 * @abstract  get and remove the first client in the queue
 * @param   lin   the linker to use
 * @param   buf   the buffer to store the client
 */
extern int linker_pop(linker *lin, client *buf);
/**
 * @function  linker_dispose
 * @abstract  free memory and destroy a linker
 * @param   linker_p    a pointer to the linker's pointer
 */
extern void linker_dispose(linker **linker_p);

#endif
