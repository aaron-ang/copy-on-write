#include "tls.h"
#include <assert.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

/*
 * This is a good place to define any data structures you will use in this file.
 * For example:
 *  - struct TLS: may indicate information about a thread's local storage
 *    (which thread, how much storage, where is the storage in memory)
 *  - struct page: May indicate a shareable unit of memory (we specified in
 *    homework prompt that you don't need to offer fine-grain cloning and CoW,
 *    and that page granularity is sufficient). Relevant information for sharing
 *    could be: where is the shared page's data, and how many threads are
 *    sharing it
 *  - Some kind of data structure to help find a TLS, searching by thread ID.
 *    E.g., a list of thread IDs and their related TLS structs, or a hash table.
 */

#define MAX_THREAD_COUNT 128

typedef struct thread_local_storage {
  pthread_t tid;
  unsigned int size;      /* size in bytes */
  unsigned int num_pages; /* number of pages */
  struct page **pages;    /* array of pointers to pages */
} TLS;

struct page {
  size_t address; /* start address of page */
  int ref_count;  /* counter for shared pages */
};

struct tid_tls_pair {
  pthread_t tid;
  TLS *tls;
};

/*
 * Now that data structures are defined, here's a good place to declare any
 * global variables.
 */

static unsigned int page_size;

static struct tid_tls_pair tid_tls_pairs[MAX_THREAD_COUNT];

/*
 * With global data declared, this is a good point to start defining your
 * static helper functions.
 */

static void register_tid_tls_pair(pthread_t tid, TLS *tls) {
  for (int i = 0; i < MAX_THREAD_COUNT; i++) {
    if (tid_tls_pairs[i].tid == 0) {
      tid_tls_pairs[i].tid = tid;
      tid_tls_pairs[i].tls = tls;
      return;
    }
  }
  fprintf(stderr, "register_tid_tls_pair: could not register pair\n");
  exit(EXIT_FAILURE);
}

static TLS *get_tls(pthread_t tid) {
  for (int i = 0; i < MAX_THREAD_COUNT; i++) {
    if (tid_tls_pairs[i].tid == tid) {
      return tid_tls_pairs[i].tls;
    }
  }
  return NULL;
}

static void tls_handle_page_fault(int sig, siginfo_t *si, void *context) {
  // Get base address of page where fault occurred
  size_t p_fault = (size_t)(si->si_addr) & ~((size_t)page_size - 1);
  // Check if fault occurred in any of the threads' TLS
  for (int i = 0; i < MAX_THREAD_COUNT; i++) {
    struct tid_tls_pair pair = tid_tls_pairs[i];
    TLS *tls = pair.tls;
    if (tls == NULL)
      continue;
    assert(pair.tid == tls->tid);
    for (int j = 0; j < tls->num_pages; j++) {
      if (tls->pages[j]->address == p_fault)
        pthread_exit(NULL);
    }
  }
  // If fault occurred outside of any TLS, terminate the process
  signal(SIGSEGV, SIG_DFL);
  signal(SIGBUS, SIG_DFL);
  raise(sig);
}

static void tls_init() {
  if (page_size == 0)
    page_size = getpagesize();

  struct sigaction sigact = {
      .sa_sigaction = tls_handle_page_fault,
      .sa_flags = SA_SIGINFO,
  };
  sigemptyset(&sigact.sa_mask);
  sigaction(SIGBUS, &sigact, NULL);
  sigaction(SIGSEGV, &sigact, NULL);
}

static void tls_protect(struct page *p) {
  if (mprotect((void *)p->address, page_size, PROT_NONE)) {
    fprintf(stderr, "tls_protect: could not protect page\n");
    exit(EXIT_FAILURE);
  }
}

static void tls_unprotect(struct page *p) {
  if (mprotect((void *)p->address, page_size, PROT_READ | PROT_WRITE)) {
    fprintf(stderr, "tls_unprotect: could not unprotect page\n");
    exit(EXIT_FAILURE);
  }
}

static struct page *create_copy(struct page *p) {
  struct page *copy = malloc(sizeof(struct page));
  if (copy == NULL) {
    fprintf(stderr, "create_copy: could not allocate memory for page\n");
    exit(EXIT_FAILURE);
  }
  copy->address = (size_t)mmap(0, page_size, PROT_WRITE,
                               MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
  copy->ref_count = 1;
  memcpy((void *)copy->address, (void *)p->address, page_size);
  p->ref_count--;
  tls_protect(p);
  return copy;
}

static TLS *clone(TLS *target) {
  TLS *lsa = malloc(sizeof(TLS));
  if (lsa == NULL) {
    fprintf(stderr, "clone: could not allocate memory for TLS\n");
    exit(EXIT_FAILURE);
  }
  lsa->tid = pthread_self();
  lsa->size = target->size;
  lsa->num_pages = target->num_pages;
  lsa->pages = calloc(lsa->num_pages, sizeof(struct page *));
  if (lsa->pages == NULL) {
    fprintf(stderr, "clone: could not allocate memory for pages\n");
    exit(EXIT_FAILURE);
  }
  for (int i = 0; i < lsa->num_pages; i++) {
    lsa->pages[i] = target->pages[i];
    lsa->pages[i]->ref_count++;
  }
  return lsa;
}

/*
 * Lastly, here is a good place to add your externally-callable functions.
 */

int tls_create(unsigned int size) {
  static bool initialized = false;
  if (initialized == false) {
    tls_init();
    initialized = true;
  }

  pthread_t tid = pthread_self();
  if (size == 0 || get_tls(tid))
    return -1;

  TLS *lsa = malloc(sizeof(TLS));
  if (lsa == NULL)
    return -1;

  lsa->tid = tid;
  lsa->size = size;
  lsa->num_pages = (size + page_size - 1) / page_size;
  lsa->pages = calloc(lsa->num_pages, sizeof(struct page *));
  if (lsa->pages == NULL) {
    free(lsa);
    return -1;
  }
  for (int i = 0; i < lsa->num_pages; i++) {
    lsa->pages[i] = malloc(sizeof(struct page));
    if (lsa->pages[i] == NULL) {
      for (int j = 0; j < i; j++)
        free(lsa->pages[j]);
      free(lsa->pages);
      free(lsa);
      return -1;
    }
    lsa->pages[i]->address = (size_t)mmap(0, page_size, PROT_NONE,
                                          MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
    lsa->pages[i]->ref_count = 1;
  }
  register_tid_tls_pair(tid, lsa);
  return 0;
}

int tls_destroy() {
  pthread_t tid = pthread_self();
  struct tid_tls_pair *pair = NULL;
  for (int i = 0; i < MAX_THREAD_COUNT; i++) {
    if (tid_tls_pairs[i].tid == tid) {
      pair = &tid_tls_pairs[i];
      break;
    }
  }
  if (pair == NULL)
    return -1;

  TLS *lsa = pair->tls;
  assert(lsa->tid == tid);

  for (int i = 0; i < lsa->num_pages; i++) {
    struct page *p = lsa->pages[i];
    if (--p->ref_count == 0) {
      munmap((void *)p->address, page_size);
      free(p);
    }
  }
  free(lsa->pages);
  free(lsa);
  pair->tid = 0;
  pair->tls = NULL;
  return 0;
}

int tls_read(unsigned int offset, unsigned int length, char *buffer) {
  TLS *lsa = get_tls(pthread_self());
  if (lsa == NULL || offset + length > lsa->size)
    return -1;

  assert(lsa->tid == pthread_self());

  unsigned int bytes_read = 0;
  while (bytes_read < length) {
    struct page *p = lsa->pages[offset / page_size];
    tls_unprotect(p);
    unsigned int page_offset = offset % page_size;
    unsigned int page_remaining = page_size - page_offset;
    unsigned int remaining_bytes = length - bytes_read;
    unsigned int bytes_to_read =
        (page_remaining < remaining_bytes) ? page_remaining : remaining_bytes;
    memcpy(buffer + bytes_read, (void *)(p->address + page_offset),
           bytes_to_read);
    tls_protect(p);
    bytes_read += bytes_to_read;
    offset += bytes_to_read;
  }
  return 0;
}

int tls_write(unsigned int offset, unsigned int length, const char *buffer) {
  TLS *lsa = get_tls(pthread_self());
  if (lsa == NULL || offset + length > lsa->size)
    return -1;

  assert(lsa->tid == pthread_self());

  unsigned int bytes_written = 0;
  while (bytes_written < length) {
    struct page *p = lsa->pages[offset / page_size];
    tls_unprotect(p);
    if (p->ref_count > 1) {
      p = lsa->pages[offset / page_size] = create_copy(p);
    }
    unsigned int page_offset = offset % page_size;
    unsigned int page_remaining = page_size - page_offset;
    unsigned int remaining_bytes = length - bytes_written;
    unsigned int bytes_to_write =
        (page_remaining < remaining_bytes) ? page_remaining : remaining_bytes;
    memcpy((void *)(p->address + page_offset), buffer + bytes_written,
           bytes_to_write);
    tls_protect(p);
    bytes_written += bytes_to_write;
    offset += bytes_to_write;
  }
  return 0;
}

int tls_clone(pthread_t tid) {
  pthread_t self_id = pthread_self();
  if (get_tls(self_id))
    return -1;

  TLS *target = get_tls(tid);
  if (target == NULL)
    return -1;

  assert(target->tid == tid);

  TLS *lsa = clone(target);
  register_tid_tls_pair(self_id, lsa);
  return 0;
}
