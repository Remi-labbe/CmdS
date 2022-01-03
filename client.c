#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <signal.h>
#include "tools/linker.h"
#include "tools/config.h"

#define STRMEMCPY(dest, src) \
    memcpy(dest, src, strlen(src) > sizeof(dest) ? sizeof(dest) : strlen(src));

void setup_signals(void);
void handler(int signum);

void help(void) {
  printf("***\nUsage:\n");
  printf("./cmdc\n");
  printf("CmdC>\ncmd arg1 ... argN\n");
  exit(EXIT_SUCCESS);
}

int main (int argc, char **argv) {
  if (argc > 1) {
      help();
      exit(EXIT_FAILURE);
  }
  setup_signals();
  pid_t pid = getpid();
  char wd_buf[WD_LEN] = { 0 };
  if (getcwd(wd_buf, WD_LEN) == NULL) {
    perror("getcwd");
    exit(EXIT_FAILURE);
  }
  // init client struct
  client c;
  c.pid = pid;
  STRMEMCPY(c.working_dir, wd_buf);
  // creates pipes to communicate with server
  char pipe_in[PIPE_LEN] = { 0 };
  snprintf(pipe_in, sizeof(pipe_in), "/tmp/%d_in", pid);
  char pipe_out[PIPE_LEN] = { 0 };
  snprintf(pipe_out, sizeof(pipe_out), "/tmp/%d_out", pid);

  // Create pipe to send data
  if (mkfifo(pipe_in, S_IRUSR | S_IWUSR) == -1) {
    perror("mkfifo");
    exit(EXIT_FAILURE);
  }

  // Create pipe to receive data

  // connect to server
  linker *lp = linker_connect(LINKER_SHM);
  if (lp == NULL) {
    fprintf(stderr, "Error: Can't connect to server.\n");
    exit(EXIT_FAILURE);
  }
  if (linker_push(lp, &c) == -1) {
    fprintf(stderr, "Error: Cant send request.");
    exit(EXIT_FAILURE);
  }

  int fd_in = open(pipe_in, O_WRONLY);
  if (fd_in == -1) {
    perror("open in");
    exit(EXIT_FAILURE);
  }

  if (unlink(pipe_in) == -1) {
    perror("unlink");
    exit(EXIT_FAILURE);
  }

  // detect streams size to create buffers
  // >IN
  struct stat st;
  if (fstat(fd_in, &st) == -1) {
    perror("fstat");
    exit(EXIT_FAILURE);
  }
  ssize_t blksize_pipe_in = st.st_blksize;

  char buf_in[blksize_pipe_in];

  // Prompt
  printf("CmdC>\n");
  ssize_t r_in;
  while ((r_in = read(STDIN_FILENO, buf_in, blksize_pipe_in)) > 0) {
    if (mkfifo(pipe_out, S_IRUSR | S_IWUSR) == -1) {
      perror("mkfifo");
      exit(EXIT_FAILURE);
    }
    // Send cmd
    if (write(fd_in, buf_in, r_in) == -1) {
      perror("write");
      exit(EXIT_FAILURE);
    }

    // Connect to response pipe
    int fd_out = open(pipe_out, O_RDONLY);
    if (fd_out == -1) {
      perror("open in");
      exit(EXIT_FAILURE);
    }
    if (unlink(pipe_out) == -1) {
      perror("unlink");
      exit(EXIT_FAILURE);
    }
    if (fstat(fd_out, &st) == -1) {
      perror("fstat");
      exit(EXIT_FAILURE);
    }
    ssize_t blksize_pipe_out = st.st_blksize;
    if (fstat(STDOUT_FILENO, &st) == -1) {
      perror("fstat");
      exit(EXIT_FAILURE);
    }
    ssize_t blksize_std_out = st.st_blksize;

    char buf_out[blksize_pipe_out];

    // Read result
    ssize_t r_out;
    while((r_out = read(fd_out, buf_out, (size_t) blksize_pipe_out)) > 0) {
      char *b = buf_out;
      do {
        ssize_t w = r_out > blksize_std_out ? blksize_std_out : r_out;
        if ((w = write(STDOUT_FILENO, b, w)) == -1) {
          perror("write");
          exit(EXIT_FAILURE);
        }

        r_out -= w;
        b += w;

      } while (r_out > 0);
    }
    if (close(fd_out) == -1) {
      perror("close");
      exit(EXIT_FAILURE);
    }
    // Prompt
    printf("CmdC>\n");
  }
  if (close(fd_in) == -1) {
    perror("close");
    exit(EXIT_FAILURE);
  }

  return EXIT_SUCCESS;
}

void setup_signals(void) {
  struct sigaction action;
  action.sa_handler = handler;
  action.sa_flags = 0;

  if (sigfillset(&action.sa_mask) == -1) {
    perror("sigfillset");
    exit(EXIT_FAILURE);
  }
  if (sigaction(SIG_FAILURE, &action, NULL) == -1) {
    perror("sigaction");
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

// signal handler
void handler(int signum) {
  if (signum == SIGINT || signum == SIGQUIT) {
    printf("Disconnecting...\n");
    exit(EXIT_SUCCESS);
  }
  if (signum == SIG_FAILURE) {
    fprintf(stderr, "Request Canceled.\n");
    exit(EXIT_FAILURE);
  }
  fprintf(stderr, "Wrong signal received [%d].\n", signum);
  exit(EXIT_FAILURE);
}
