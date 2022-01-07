#ifdef _XOPEN_SOURCE
#undef _XOPEN_SOURCE
#define _XOPEN_SOURCE 500
#endif
#include <stdio.h>
#include <stdlib.h>

#include <fcntl.h>
#include <semaphore.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "config.h"
#include "linker.h"

struct linker {
  const char *shm_name;
  size_t head;
  size_t tail;
  sem_t mutex;
  sem_t empty;
  sem_t full;
  char buffer[];
};

/**
 * @function  _cleanup
 * @abstract  free the memory and destroy linker's shm
 * @param   lp    linker pointer
 */
static void _cleanup(linker *lp) {
  if (sem_destroy(&lp->mutex) == -1) {
    perror("sem_destroy - mutex");
  }
  if (sem_destroy(&lp->full) == -1) {
    perror("sem_destroy - full");
  }
  if (sem_destroy(&lp->empty) == -1) {
    perror("sem_destroy - empty");
  }
  if (shm_unlink(lp->shm_name) == -1) {
    perror("shm_unlink");
  }
}

linker *linker_init(const char *name) {
  size_t shm_size = sizeof(linker) + CAPACITY * sizeof(client);

  int fd = shm_open(name, O_RDWR | O_CREAT | O_EXCL, S_IRUSR | S_IWUSR);

  if (fd == -1) {
    perror("shm_open");
    return NULL;
  }

  if (ftruncate(fd, (off_t)shm_size) == -1) {
    perror("ftruncate");
    return NULL;
  }

  struct linker *lp =
      mmap(NULL, shm_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

  if (lp == NULL) {
    perror("mmap");
    return NULL;
  }

  close(fd);

  lp->head = 0;
  lp->tail = 0;
  lp->shm_name = name;

  if (sem_init(&lp->mutex, 1, 1) == -1) {
    perror("sem_init - mutex");
    _cleanup(lp);
    return NULL;
  }

  if (sem_init(&lp->full, 1, 0) == -1) {
    perror("sem_init - full");
    _cleanup(lp);
    return NULL;
  }

  if (sem_init(&lp->empty, 1, CAPACITY) == -1) {
    perror("sem_init - empty");
    _cleanup(lp);
    return NULL;
  }

  return lp;
}

linker *linker_connect(const char *name) {
  int fd = shm_open(name, O_RDWR, S_IRUSR | S_IWUSR);

  if (fd == -1) {
    perror("shm_open");
    return NULL;
  }

  struct stat ss;

  if (fstat(fd, &ss) == -1) {
    perror("fstat");
    return NULL;
  }

  struct linker *lp =
      mmap(NULL, (size_t)ss.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

  if (lp == MAP_FAILED) {
    perror("mmap");
    return NULL;
  }

  close(fd);

  return lp;
}

#define FUN_FAILURE -1
#define FUN_SUCCESS 0

int linker_push(linker *lin, const client *c) {
  if (lin == NULL || c == NULL) {
    return FUN_FAILURE;
  }

  if (sem_wait(&lin->empty) == -1) {
    perror("sem_wait");
    return FUN_FAILURE;
  }

  if (sem_wait(&lin->mutex) == -1) {
    perror("sem_wait");
    return FUN_FAILURE;
  }

  memcpy(lin->buffer + lin->head * sizeof(client), c, sizeof(client));
  lin->head = (lin->head + 1) % CAPACITY;

  if (sem_post(&lin->mutex)) {
    perror("sem_post");
    return FUN_FAILURE;
  }

  if (sem_post(&lin->full)) {
    perror("sem_post");
    return FUN_FAILURE;
  }

  return FUN_SUCCESS;
}

int linker_pop(linker *lin, client *buf) {
  if (lin == NULL || buf == NULL) {
    return FUN_FAILURE;
  }

  if (sem_wait(&lin->full) == -1) {
    perror("sem_wait");
    return FUN_FAILURE;
  }

  if (sem_wait(&lin->mutex) == -1) {
    perror("sem_wait");
    return FUN_FAILURE;
  }

  memcpy(buf, lin->buffer + lin->tail * sizeof(client), sizeof(client));
  lin->tail = (lin->tail + 1) % CAPACITY;

  if (sem_post(&lin->mutex)) {
    perror("sem_post");
    return FUN_FAILURE;
  }

  if (sem_post(&lin->empty)) {
    perror("sem_post");
    return FUN_FAILURE;
  }

  return FUN_SUCCESS;
}

void linker_dispose(linker **linker_p) {
  _cleanup(*linker_p);
  *linker_p = NULL;
}
