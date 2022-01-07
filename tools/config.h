#ifndef CONFIG__H
#define CONFIG__H

/**
* @define CAPACITY  the capacity of the launcher: size of queue and nb of threads
*/
#ifndef CAPACITY
#define CAPACITY 10
#endif

/**
* @define LINKER_SHM Name of the shm in which we store the linker
*/
#ifndef LINKER_SHM
#define LINKER_SHM "/shm_my_linker_1207"
#endif

/**
* @define PIPE_LEN the max length of a pipe name
*/
#ifndef PIPE_LEN
#define PIPE_LEN 128
#endif

/**
* @define SIG_FAILURE Signal sent in case of error
*/
#ifndef SIG_FAILURE
#define SIG_FAILURE SIGUSR1
#endif

/**
* @define SIG_SUCCESS Signal sent in case of success
*/
#ifndef SIG_SUCCESS
#define SIG_SUCCESS SIGUSR2
#endif

#endif
