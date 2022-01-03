#ifndef CONFIG__H
#define CONFIG__H

#ifndef CAPACITY
#define CAPACITY 10
#endif

#ifndef LINKER_SHM
#define LINKER_SHM "/shm_my_linker_1207"
#endif

#ifndef PIPE_LEN
#define PIPE_LEN 128
#endif

#ifndef SIG_FAILURE
#define SIG_FAILURE SIGUSR1
#endif

#ifndef SIG_SUCCESS
#define SIG_SUCCESS SIGUSR2
#endif

#endif
