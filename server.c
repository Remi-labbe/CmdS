#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <string.h>
#include <fcntl.h>
#include <time.h>
#include <stdarg.h>
#include <errno.h>
#include "linker/linker.h"
#include "linker/config.h"

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
void quit(const char *fmt, ...);
void cleanup(void);
void listen(void);

// Threads related
void start_th(size_t i, client c);
void *runner_routine(struct runner *r);
size_t count_args(const char *str);
void fmt_args(const char *str, char *argv[], char *buf);

// Signal Handler
void setup_signals(void);
void handler(int signum);

/* Global scoped variables */

static struct runner *runner_pool;
static linker *lin;

// MAIN

int main(void) {
  setup_signals();

  listen();

  exit(EXIT_SUCCESS);
}

// AUXILIARY FUNCS

/**
 * @function  cleanup
 * @abstract  clean the memory
 */
void cleanup(void) {
  if (runner_pool != NULL) {
    for (size_t i = 0; i < CAPACITY; i++) {
      struct runner rnr = runner_pool[i];
      if (rnr.running) {
        pthread_cancel(rnr.th);
        pthread_join(rnr.th, NULL);
        kill(rnr.clt.pid, SIG_FAILURE);
        struct timespec end;
        if (clock_gettime(CLOCK_REALTIME, &end) == -1) {
          perror("clock_gettime");
          exit(EXIT_FAILURE);
        }
        if (end.tv_nsec < rnr.start_t.tv_nsec) {
          --end.tv_sec;
          end.tv_nsec += 1000000000;
        }
        time_t sec = end.tv_sec - rnr.start_t.tv_sec;
        long nsec = end.tv_nsec - rnr.start_t.tv_nsec;

        long diff_ms = sec * 1000 + nsec / 1000000;
        printf("- Killed client[%d] connected for %ldms\n", rnr.clt.pid, diff_ms);
      }
    }
  }
  if (lin != NULL) {
    linker_dispose(&lin);
  }
}

/**
 * @function  quit
 * @abstract  Terminate the program in case of error printing infos in args
 * @param     fmt      format of the error string
 * @param     ...      args associated to the string
 */
void quit(const char *fmt, ...) {
  const char *err = strerror(errno);

  // print args into buffer
  va_list args_list;
  va_start(args_list, fmt);
  char buf[1024] = { 0 };
  vsnprintf(buf, sizeof(buf), fmt, args_list);

  // format with error code
  fprintf(stderr, "Server quit: %s [%s]\n", buf, err);
  va_end(args_list);

  // shutdown server
  cleanup();
  exit(EXIT_FAILURE);
}

/**
 * @function  setup_signals
 * @abstract  Setup program behaviour regarding signals
 */
void setup_signals(void) {
  struct sigaction action;
  action.sa_handler = handler;
  action.sa_flags = 0;

  if (sigfillset(&action.sa_mask) == -1) {
    perror("sigfillset");
    exit(EXIT_FAILURE);
  }

  if (sigaction(SIGINT, &action, NULL) == -1) {
    perror("sigaction");
    exit(EXIT_FAILURE);
  }
  if (sigaction(SIGQUIT, &action, NULL) == -1) {
    perror("sigaction");
    exit(EXIT_FAILURE);
  }
}

/**
 * @function  listen
 * @abstract  Main loop of the program, listen for requests
 */
void listen(void) {
  lin = linker_init(SHM_NAME);
  if (lin == NULL) {
    fprintf(stderr, "Error: Couldn't create linker.\n");
    exit(EXIT_FAILURE);
  }

  struct runner rnrs[CAPACITY];
  runner_pool = rnrs;

  for (size_t i = 0; i < CAPACITY; i++) {
    rnrs[i].id = i;
    rnrs[i].clt.pid = 0;
    rnrs[i].running = false;
  }

  client c;
  while (linker_pop(lin, &c) == 0) {
    printf("Popped request from [%d]\n", c.pid);

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
  printf("%d\n", __LINE__);
}

/**
 * @function  start_th
 * @abstract  Start the ith thread in the runner_pool binding it to the client c
 * @param     i      index if the runner to start
 * @param     c      client to bind to the runner
 */
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
          (void *(*)(void *)) runner_routine, &runner_pool[i])) != 0) {
    quit("pthread_create: %s", strerror(r));
  }
}

/**
 * @function  runner_routine
 * @abstract  Routine ran by a thread when it is listening to a client
 * @param     r       the runner associated to the thread
 */
void *runner_routine(struct runner *r) {
  if (clock_gettime(CLOCK_REALTIME, &r->start_t) == -1) {
    perror("clock_gettime");
    exit(EXIT_FAILURE);
  }

  printf("+ Started client[%d] on thread[%zu]\n", r->clt.pid, r->id);
  char pipe_in[PIPE_LEN] = { 0 };
  snprintf(pipe_in, sizeof(pipe_in), "/tmp/%d_in", r->clt.pid);
  char pipe_out[PIPE_LEN] = { 0 };
  snprintf(pipe_out, sizeof(pipe_out), "/tmp/%d_out", r->clt.pid);

  if (chdir(r->clt.working_dir) == -1) {
    perror("chdir");
    exit(EXIT_FAILURE);
  }

  int fd_in = open(pipe_in, O_RDONLY);
  if (fd_in == -1) {
    perror("open in");
    exit(EXIT_FAILURE);
  }
  struct stat st;
  if (fstat(fd_in, &st) == -1) {
    perror("fstat");
    exit(EXIT_FAILURE);
  }
  ssize_t blksize_pipe_in = st.st_blksize;

  char buf_in[blksize_pipe_in];

  ssize_t r_in;
  while ((r_in = read(fd_in, buf_in, (size_t) blksize_pipe_in)) > 0) {
    // Removing line break at the end of input
    if (buf_in[strlen(buf_in) - 1] == '\n') {
      buf_in[strlen(buf_in) - 1] = 0;
    }
    printf("[%zu] received cmd:%s from [%d]\n",r->id, buf_in, r->clt.pid);
    char *argv[count_args(buf_in) + 1];
    char fmt_buf[strlen(buf_in) + 1];
    int status, fd_out;
    struct timespec start;
    if (clock_gettime(CLOCK_REALTIME, &start) == -1) {
      perror("clock_gettime");
      exit(EXIT_FAILURE);
    }

    // Fork / Exec
    switch (fork()) {
      case -1:
        perror("fork");
        break;

     case 0:
        fd_out = open(pipe_out, O_WRONLY);
        if (fd_out == -1) {
          perror("open");
          exit(EXIT_FAILURE);
        }
        if (dup2(fd_out, STDOUT_FILENO) == -1) {
          perror("dup2");
          exit(EXIT_FAILURE);
        }
        if (close(fd_out) == -1) {
          perror("close");
          exit(EXIT_FAILURE);
        }

        fmt_args(buf_in, argv, fmt_buf);

        execvp(argv[0], argv);

        perror("execvp");
        exit(EXIT_FAILURE);
      default:
        wait(&status);
        break;
    }


    // Checking if exec was successful
    if (WEXITSTATUS(status) == EXIT_FAILURE) {
      fprintf(stderr, "thread[%zu]: Failed to execute cmd: %s\nDisconnecting client[%d]\n",
        r->id, buf_in, r->clt.pid);
      if (kill(r->clt.pid, SIG_FAILURE)) {
        perror("kill");
      }
      break;
    } else {
      struct timespec end;
      if (clock_gettime(CLOCK_REALTIME, &end) == -1) {
        perror("clock_gettime");
        exit(EXIT_FAILURE);
      }
      if (end.tv_nsec < start.tv_nsec) {
        --end.tv_sec;
        end.tv_nsec += 1000000000;
      }
      time_t sec = end.tv_sec - start.tv_sec;
      long nsec = end.tv_nsec - start.tv_nsec;

      long diff_ms = sec * 1000 + nsec / 1000000;
      printf("[%zu] Finnished executing cmd: [%s] for client[%d] in %ldms\n",
        r->id, buf_in, r->clt.pid, diff_ms);
      for (int i = 0; i < blksize_pipe_in; i++)
      buf_in[i] = 0;
    }
  }

  struct timespec end;
  if (clock_gettime(CLOCK_REALTIME, &end) == -1) {
    perror("clock_gettime");
    exit(EXIT_FAILURE);
  }
  if (end.tv_nsec < r->start_t.tv_nsec) {
    --end.tv_sec;
    end.tv_nsec += 1000000000;
  }
  time_t sec = end.tv_sec - r->start_t.tv_sec;
  long nsec = end.tv_nsec - r->start_t.tv_nsec;

  long diff_ms = sec * 1000 + nsec / 1000000;
  printf("- Stopped client[%d] on thread[%zu] connection lasted: %ldms\n",
    r->clt.pid, r->id, diff_ms);
  r->running = false;

  return NULL;
}

/**
 * @function  count_args
 * @abstract  count words in a string separated by ' '
 * @param     str       the string in which the function will count words
 * @result    size_t    the number of word(s)
 */
size_t count_args(const char *str) {
  size_t i = 0;
  size_t count = 0;
  bool inw = true;
  for(i = 0; str[i]; i++) {
    if(str[i] == ' ' && inw) {
      count++;
      inw = false;
    } else if (str[i] != ' ') {
      inw = true;
    }
  }
  if(i > 0)
    count++;
  return count;
}

/**
 * @function  fmt_args
 * @abstract  put a sliced version of the string str in argv
 * @param     str       the string to slice !Not modified
 * @param     argv      the array buffer in which we place words
 * @param     buf       buffer used to keep words separated by null char
 */
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

// signal handler
/**
 * @function  handler
 * @abstract  handle signals
 * @param     signum    signal received
 */
void handler(int signum) {
  if (signum < 0) {
    quit("Wrong signal number: %d\n", signum);
  }
  cleanup();
  printf("Shuting down server [%d]\n", signum);
  exit(EXIT_SUCCESS);
}
