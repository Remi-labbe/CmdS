#include "tools/config.h"
#include "tools/linker.h"
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <semaphore.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/syslog.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

/**
 * @define  DAEMON_PID_SHM    shm's name in which we store the daemon pid
 */
#ifndef DAEMON_PID_SHM
#define DAEMON_PID_SHM "/cmds_daemon_pid"
#endif

/**
 * @define  START             string "start"
 */
#ifndef START
#define START "start"
#endif

/**
 * #define  STOP              string "stop"
 */
#ifndef STOP
#define STOP "stop"
#endif

#define TESTOPT(opt) strcmp(opt, argv[1]) == 0

/**
 * @struct    runner
 * @abstract  struct associated to a thread to describe client and stuff
 *
 * @field     id        unique id
 * @field     clt       associated client
 * @field     th        allocated thread
 * @field     running   is this runner working?
 * @field     start_t   time runner start working
 */
struct runner {
  size_t id;
  client clt;
  pthread_t th;
  bool running;
  struct timespec start_t;
};

/* Functions declarations */

// General server function
/**
 * @function  quit
 * @abstract  Terminate the program in case of error printing infos in args
 * @param     fmt      format of the error string
 * @param     ...      args associated to the string
 */
void quit(const char *fmt, ...);
/**
 * @function  cleanup
 * @abstract  clean the memory
 */
void cleanup(void);
/**
 * @function  count_args
 * @abstract  count words in a string separated by ' '
 * @param     str       the string in which the function will count words
 * @result    size_t    the number of word(s)
 */
size_t count_args(const char *str);
/**
 * @function  fmt_args
 * @abstract  put a sliced version of the string str in argv
 * @param     str       the string to slice !Not modified
 * @param     argv      the array buffer in which we place words
 * @param     buf       buffer used to keep words separated by null char
 */
void fmt_args(const char *str, char *argv[], char *buf);

// daemon handling
/**
 * @function  isRunning
 * @abstract  Checks if daemon is already running
*/
bool isRunning(void);
/**
 * @function  store_dpid
 * @abstract  Main loop of the program, listen for requests
*/
pid_t store_dpid(void);
/**
 * @function  daemon_main
 * @abstract  Stores the daemon pid in the shm of name DAEMON_PID_SHM
*/
pid_t get_dpid(void);
/**
 * @function  create_daemon
 * @abstract  Setup the daemon then start daemon_main
 * @param     ppid    the starter process pid
*/
void create_daemon(pid_t ppid);
/**
 * @function  daemon_main
 * @abstract  Main loop of the program, listen for requests
 * @param     starter_pid    the starter process pid
*/
void daemon_main(pid_t starter_pid);

// Threads related
/**
 * @function  start_th
 * @abstract  Start the ith thread in the runner_pool binding it to the client c
 * @param     i      index if the runner to start
 * @param     c      client to bind to the runner
 */
void start_th(size_t i, client c);
/**
 * @function  runner_routine
 * @abstract  Routine ran by a thread when it is listening to a client
 * @param     r       the runner associated to the thread
 */
void *runner_routine(struct runner *r);

// Signal Handler
/**
 * @function  handler
 * @abstract  handle signals
 * @param     signum    signal received
 */
void handler(int signum);

/* Global scoped variables */

static struct runner *runner_pool;
static linker *lin;

// MAIN
/**
 * @function  help
 * @abstract  show heplp and exit
 */
void help(void) {
  printf("***\nUsage:\n");
  printf("./cmds [start|stop]\n");
  exit(EXIT_SUCCESS);
}

int main(int argc, char **argv) {
  if (argc < 2 || !(TESTOPT(START) || TESTOPT(STOP))) {
    help();
  }

  // test if daemon running
  bool running = isRunning();
  if (TESTOPT(START) && running) {
    fprintf(stderr, "Error: Server is already running.\n");
    exit(EXIT_FAILURE);
  } else if (TESTOPT(STOP)) {
    if (!running) {
      fprintf(stderr, "Error: Server is not running.\n");
      exit(EXIT_FAILURE);
    }
    // stop the daemon
    pid_t pid;
    if ((pid = get_dpid()) == -1) {
      fprintf(stderr, "Can't get daemon pid\n");
      exit(EXIT_FAILURE);
    }
    if (kill(pid, SIGTERM) == -1) {
      perror("kill");
      exit(EXIT_FAILURE);
    }
    exit(EXIT_SUCCESS);
  }

  // Open logger
  openlog("cmds", LOG_PID, LOG_DAEMON);

  // close all open file descriptors starting at 3
  // keeping stdin/out/err
  for (int i = 3; i < RLIMIT_NOFILE; i++) {
    if (close(i) == -1 && errno == EBADF) {
      // stop if the file descriptor i is invalid
      break;
    }
  }
  // ignore all signals
  sigset_t set;
  if (sigfillset(&set) == -1) {
    quit("sigfillset");
  }
  if (sigprocmask(SIG_SETMASK, &set, NULL) == -1) {
    quit("sigprocmask");
  }
  pid_t ppid = getpid();
  struct sigaction action;
  action.sa_handler = handler;
  action.sa_flags = 0;
  // fork once
  switch (fork()) {
  case -1:
    quit("fork");
  case 0:
    create_daemon(ppid);
    break;
  default:
    // father waits for child signal SUCCESS/FAILURE
    if (sigfillset(&action.sa_mask) == -1) {
      quit("sigfillset");
    }
    if (sigaction(SIG_SUCCESS, &action, NULL) == -1) {
      quit("sigfillset");
    }
    if (sigaction(SIG_FAILURE, &action, NULL) == -1) {
      quit("sigfillset");
    }
    if (sigdelset(&set, SIG_FAILURE) == -1) {
      quit("sigdelset");
    }
    if (sigdelset(&set, SIG_SUCCESS) == -1) {
      quit("sigdelset");
    }
    if (sigprocmask(SIG_SETMASK, &set, NULL) == -1) {
      quit("sigprocmask");
    }
    sigsuspend(&set);
  }
  exit(EXIT_SUCCESS);
}

// AUXILIARY FUNCS

bool isRunning(void) {
  return shm_open(DAEMON_PID_SHM, O_RDONLY, S_IRUSR) != -1;
}

pid_t store_dpid(void) {
  int shm_fd =
      shm_open(DAEMON_PID_SHM, O_RDWR | O_CREAT | O_EXCL, S_IRUSR | S_IWUSR);
  if (shm_fd == -1) {
    return -1;
  }
  if (ftruncate(shm_fd, sizeof(pid_t)) == -1) {
    return -1;
  }

  pid_t *shm_pid = mmap(NULL, sizeof(pid_t), PROT_WRITE, MAP_SHARED, shm_fd, 0);
  if (shm_pid == MAP_FAILED) {
    return -1;
  }

  *shm_pid = getpid();

  return *shm_pid;
}

pid_t get_dpid(void) {
  int shm_fd = shm_open(DAEMON_PID_SHM, O_RDONLY, S_IRUSR);
  if (shm_fd == -1) {
    return -1;
  }

  pid_t *shm_pid = mmap(NULL, sizeof(pid_t), PROT_READ, MAP_SHARED, shm_fd, 0);
  if (shm_pid == MAP_FAILED) {
    return -1;
  }

  return *shm_pid;
}

void create_daemon(pid_t ppid) {
  // detach from tty
  if (setsid() == -1) {
    if (kill(ppid, SIG_FAILURE) == -1) {
      quit("kill");
    }
    quit("setsid");
  }

  // fork again to ensure the daemon does not get a tty back
  switch (fork()) {
  case -1:
    if (kill(ppid, SIG_FAILURE) == -1) {
      quit("kill");
    }
    quit("second fork");
  case 0:
    // reset mask to 0
    umask(0);

    // change directory to root(/)
    if (chdir("/") == -1) {
      if (kill(ppid, SIG_FAILURE) == -1) {
        quit("kill");
      }
      quit("chdir");
    }

    // redirect stdin/out/err to /dev/null
    int fdnull = open("/dev/null", O_RDWR);
    if (fdnull == -1) {
      if (kill(ppid, SIG_FAILURE) == -1) {
        quit("kill");
      }
      quit("open");
    }
    if (dup2(fdnull, STDIN_FILENO) == -1) {
      if (kill(ppid, SIG_FAILURE) == -1) {
        quit("kill");
      }
      quit("dup2");
    }
    if (dup2(fdnull, STDOUT_FILENO) == -1) {
      if (kill(ppid, SIG_FAILURE) == -1) {
        quit("kill");
      }
      quit("dup2");
    }
    if (dup2(fdnull, STDERR_FILENO) == -1) {
      if (kill(ppid, SIG_FAILURE) == -1) {
        quit("kill");
      }
      quit("dup2");
    }
    if (close(fdnull) == -1) {
      if (kill(ppid, SIG_FAILURE) == -1) {
        quit("kill");
      }
      quit("close");
    }
    if (store_dpid() == -1) {
      if (kill(ppid, SIG_FAILURE) == -1) {
        quit("kill");
      }
      quit("store_dpid");
    }
    struct sigaction action;
    action.sa_handler = handler;
    action.sa_flags = 0;

    if (sigfillset(&action.sa_mask) == -1) {
      if (kill(ppid, SIG_FAILURE) == -1) {
        quit("kill");
      }
      quit("sigfillset");
    }
    if (sigaction(SIGTERM, &action, NULL) == -1) {
      if (kill(ppid, SIG_FAILURE) == -1) {
        quit("kill");
      }
      quit("sigaction");
    }
    sigset_t masked;
    if (sigfillset(&masked) == -1) {
      if (kill(ppid, SIG_FAILURE) == -1) {
        quit("kill");
      }
      quit("sigfillset");
    }
    if (sigdelset(&masked, SIGTERM) == -1) {
      if (kill(ppid, SIG_FAILURE) == -1) {
        quit("kill");
      }
      quit("sigdelset");
    }
    if (sigprocmask(SIG_SETMASK, &masked, NULL) == -1) {
      if (kill(ppid, SIG_FAILURE) == -1) {
        quit("kill");
      }
      quit("sigprocmask");
    }
    daemon_main(ppid);
    exit(EXIT_SUCCESS);
  default:
    // kill the first child
    // this ensure the child (aka the daemon) ppid is 1 (init)
    exit(EXIT_SUCCESS);
  }
}

void cleanup(void) {
  if (runner_pool != NULL) {
    for (size_t i = 0; i < CAPACITY; i++) {
      struct runner rnr = runner_pool[i];
      if (rnr.running) {
        pthread_cancel(rnr.th);
        pthread_join(rnr.th, NULL);
        kill(rnr.clt.pid, SIG_FAILURE);
        syslog(LOG_INFO, "[cmds] - Killed client[%d]", rnr.clt.pid);
      }
    }
  }
  closelog();
  if (lin != NULL) {
    linker_dispose(&lin);
  }
  shm_unlink(DAEMON_PID_SHM);
}

void quit(const char *fmt, ...) {
  const char *err = strerror(errno);

  // print args into buffer
  va_list args_list;
  va_start(args_list, fmt);
  char buf[1024] = {0};
  vsnprintf(buf, sizeof(buf), fmt, args_list);
  va_end(args_list);

  // format with error code
  fprintf(stderr, "Server quit: %s [%s]\n", buf, err);
  syslog(LOG_ERR, "[cmds] Server quit: %s [%s]", buf, err);

  // shutdown server
  cleanup();
  exit(EXIT_FAILURE);
}

void daemon_main(pid_t starter_pid) {
  lin = linker_init(LINKER_SHM);
  if (lin == NULL) {
    if (kill(starter_pid, SIG_FAILURE) == -1) {
      quit("kill");
    }
    quit("linker_init");
  }

  struct runner rnrs[CAPACITY];
  runner_pool = rnrs;

  for (size_t i = 0; i < CAPACITY; i++) {
    rnrs[i].id = i;
    rnrs[i].clt.pid = 0;
    rnrs[i].running = false;
  }

  // Tell starter process the daemon started successfully
  if (kill(starter_pid, SIG_SUCCESS) == -1) {
    quit("kill");
  }

  client c;
  while (linker_pop(lin, &c) == 0) {
    syslog(LOG_INFO, "[cmds] Popped request from [%d]", c.pid);

    bool found = false;

    for (size_t i = 0; i < CAPACITY; i++) {
      if (runner_pool[i].running == false) {
        found = true;
        start_th(i, c);
        break;
      }
    }

    if (!found) {
      if (kill(c.pid, SIG_FAILURE) == -1) {
        quit("kill");
      }
    }
  }
}

void start_th(size_t i, client c) {
  memcpy(&runner_pool[i].clt, &c, sizeof(client));
  runner_pool[i].running = true;
  pthread_attr_t attr;
  int r;
  if ((r = pthread_attr_init(&attr)) != 0) {
    quit("pthread_attr_init: %s", strerror(r));
  }
  if ((r = pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED)) != 0) {
    quit("pthread_attr_setdetachstate: %s", strerror(r));
  }
  if ((r = pthread_create(&runner_pool[i].th, &attr,
                          (void *(*)(void *))runner_routine,
                          &runner_pool[i])) != 0) {
    quit("pthread_create: %s", strerror(r));
  }
}

void *runner_routine(struct runner *r) {
  errno = 0;
  if (clock_gettime(CLOCK_REALTIME, &r->start_t) == -1) {
    syslog(LOG_ERR, "[cmds] [%zu] clock_gettime: %s", r->id, strerror(errno));
    r->running = false;
    exit(EXIT_FAILURE);
  }

  syslog(LOG_INFO, "[cmds] + Started client[%d] on thread[%zu]", r->clt.pid,
         r->id);
  char pipe_in[PIPE_LEN] = {0};
  snprintf(pipe_in, sizeof(pipe_in), "/tmp/%d_in", r->clt.pid);
  char pipe_out[PIPE_LEN] = {0};
  snprintf(pipe_out, sizeof(pipe_out), "/tmp/%d_out", r->clt.pid);

  if (chdir(r->clt.working_dir) == -1) {
    syslog(LOG_ERR, "[cmds] [%zu] chdir: %s", r->id, strerror(errno));
    r->running = false;
    exit(EXIT_FAILURE);
  }

  int fd_in = open(pipe_in, O_RDONLY);
  if (fd_in == -1) {
    syslog(LOG_ERR, "[cmds] [%zu] open: %s", r->id, strerror(errno));
    r->running = false;
    exit(EXIT_FAILURE);
  }
  struct stat st;
  if (fstat(fd_in, &st) == -1) {
    syslog(LOG_ERR, "[cmds] [%zu] fstat: %s", r->id, strerror(errno));
    r->running = false;
    exit(EXIT_FAILURE);
  }
  ssize_t blksize_pipe_in = st.st_blksize;

  char buf_in[blksize_pipe_in];

  ssize_t r_in;
  while ((r_in = read(fd_in, buf_in, (size_t)blksize_pipe_in)) > 0) {
    // Removing line break at the end of input
    if (buf_in[strlen(buf_in) - 1] == '\n') {
      buf_in[strlen(buf_in) - 1] = 0;
    }
    syslog(LOG_INFO, "[cmds] [%zu] received cmd:%s from [%d]", r->id, buf_in,
           r->clt.pid);
    char *argv[count_args(buf_in) + 1];
    char fmt_buf[strlen(buf_in) + 1];
    int status, fd_out;
    struct timespec start;
    if (clock_gettime(CLOCK_REALTIME, &start) == -1) {
      syslog(LOG_ERR, "[cmds] [%zu] clock_gettime: %s", r->id, strerror(errno));
      r->running = false;
      exit(EXIT_FAILURE);
    }

    // Fork / Exec
    switch (fork()) {
    case -1:
      syslog(LOG_ERR, "[cmds] [%zu] fork: %s", r->id, strerror(errno));
      r->running = false;
      exit(EXIT_FAILURE);
    case 0:
      fd_out = open(pipe_out, O_WRONLY);
      if (fd_out == -1) {
        syslog(LOG_ERR, "[cmds] [%zu] open: %s", r->id, strerror(errno));
        r->running = false;
        exit(EXIT_FAILURE);
      }
      if (dup2(fd_out, STDOUT_FILENO) == -1) {
        syslog(LOG_ERR, "[cmds] [%zu] dup2: %s", r->id, strerror(errno));
        r->running = false;
        exit(EXIT_FAILURE);
      }
      if (close(fd_out) == -1) {
        syslog(LOG_ERR, "[cmds] [%zu] close: %s", r->id, strerror(errno));
        r->running = false;
        exit(EXIT_FAILURE);
      }

      fmt_args(buf_in, argv, fmt_buf);

      execvp(argv[0], argv);

      syslog(LOG_ERR, "[cmds] [%zu] execvp: %s", r->id, strerror(errno));
      r->running = false;
      exit(EXIT_FAILURE);
    default:
      wait(&status);
      break;
    }

    // Checking if exec was successful
    if (WEXITSTATUS(status) == EXIT_FAILURE) {
      syslog(LOG_ERR, "[cmds] [%zu] Failed to execute cmd: [%s]", r->id,
             buf_in);
      if (kill(r->clt.pid, SIG_FAILURE)) {
        syslog(LOG_ERR, "[cmds] [%zu] clock_gettime: %s", r->id,
               strerror(errno));
        r->running = false;
        exit(EXIT_FAILURE);
      }
      break;
    } else {
      struct timespec end;
      if (clock_gettime(CLOCK_REALTIME, &end) == -1) {
        syslog(LOG_ERR, "[cmds] [%zu] clock_gettime: %s", r->id,
               strerror(errno));
        r->running = false;
        exit(EXIT_FAILURE);
      }
      if (end.tv_nsec < start.tv_nsec) {
        --end.tv_sec;
        end.tv_nsec += 1000000000;
      }
      time_t sec = end.tv_sec - start.tv_sec;
      long nsec = end.tv_nsec - start.tv_nsec;

      long diff_ms = sec * 1000 + nsec / 1000000;
      syslog(
          LOG_INFO,
          "[cmds] [%zu] Finnished executing cmd: [%s] for client[%d] in %ldms",
          r->id, buf_in, r->clt.pid, diff_ms);
      for (int i = 0; i < blksize_pipe_in; i++)
        buf_in[i] = 0;
    }
  }

  struct timespec end;
  if (clock_gettime(CLOCK_REALTIME, &end) == -1) {
    syslog(LOG_ERR, "[cmds] [%zu] clock_gettime: %s", r->id, strerror(errno));
    r->running = false;
    exit(EXIT_FAILURE);
  }
  if (end.tv_nsec < r->start_t.tv_nsec) {
    --end.tv_sec;
    end.tv_nsec += 1000000000;
  }
  time_t sec = end.tv_sec - r->start_t.tv_sec;
  long nsec = end.tv_nsec - r->start_t.tv_nsec;

  long diff_ms = sec * 1000 + nsec / 1000000;
  syslog(LOG_INFO,
         "[cmds] - Stopped client[%d] on thread[%zu] connection lasted: %ldms",
         r->clt.pid, r->id, diff_ms);
  r->running = false;

  return NULL;
}

size_t count_args(const char *str) {
  size_t i = 0;
  size_t count = 0;
  bool inw = true;
  for (i = 0; str[i]; i++) {
    if (str[i] == ' ' && inw) {
      count++;
      inw = false;
    } else if (str[i] != ' ') {
      inw = true;
    }
  }
  if (i > 0)
    count++;
  return count;
}

void fmt_args(const char *str, char *argv[], char *buf) {
  memcpy(buf, str, strlen(str) + 1);

  size_t i = 0, j = 0;

  argv[j++] = buf;

  while (j < count_args(str)) {
    if (str[i] == ' ') {
      buf[i] = 0;
      argv[j++] = buf + i + 1;
    }
    i++;
  }

  argv[j] = NULL;
}

void handler(int signum) {
  if (signum == SIGTERM) {
    syslog(LOG_INFO, "[cmds] Daemon Stopped");
    cleanup();
    exit(EXIT_SUCCESS);
  } else if (signum == SIG_SUCCESS) {
    printf("[cmds] Started cmds Daemon successfully\n");
    exit(EXIT_SUCCESS);
  } else if (signum == SIG_FAILURE) {
    quit("Failed to start Daemon");
  }
  quit("wrong signal [%d]", signum);
}
