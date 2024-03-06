#include "tls.h"
#include <assert.h>
#include <search.h>
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
  unsigned int address; /* start address of page */
  int ref_count;        /* counter for shared pages */
};

ENTRY e, *ep;

/*
 * Now that data structures are defined, here's a good place to declare any
 * global variables.
 */

int num_tls;

bool hash_initialized;

unsigned int page_size;

pthread_t threads[MAX_THREAD_COUNT];

/*
 * With global data declared, this is a good point to start defining your
 * static helper functions.
 */

static void h_init() {
  if (hcreate(MAX_THREAD_COUNT) == 0) {
    fprintf(stderr, "h_init: could not create hash table\n");
    exit(EXIT_FAILURE);
  }
  hash_initialized = true;
}

static void h_update(pthread_t tid, TLS *tls) {
  e.key = (char *)tid;
  e.data = tls;
  if (hsearch(e, ENTER) == NULL) {
    fprintf(stderr, "hadd: entry failed\n");
    exit(EXIT_FAILURE);
  }
}

static TLS *h_get(pthread_t tid) {
  e.key = (char *)tid;
  if ((ep = hsearch(e, FIND)) == NULL)
    return NULL;
  return ep->data;
}

static void tls_handle_page_fault(int sig, siginfo_t *si, void *context) {
  // Get base address of page where fault occurred
  unsigned int p_fault = ((size_t)si->si_addr) & ~(page_size - 1);
  // Check if fault occurred in any of the threads' TLS
  for (int i = 0; i < num_tls; i++) {
    pthread_t tid = threads[i];
    if (tid == pthread_self() || tid == 0)
      continue;

    TLS *tls = h_get(tid);
    if (tls == NULL)
      continue;

    assert(tls->tid == tid);

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
  if (hash_initialized == false)
    h_init();

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
  if (mprotect((void *)(size_t)p->address, page_size, PROT_NONE)) {
    fprintf(stderr, "tls_protect: could not protect page\n");
    exit(EXIT_FAILURE);
  }
}

static void tls_unprotect(struct page *p) {
  if (mprotect((void *)(size_t)p->address, page_size, PROT_READ | PROT_WRITE)) {
    fprintf(stderr, "tls_unprotect: could not unprotect page\n");
    exit(EXIT_FAILURE);
  }
}

void register_tid(pthread_t tid) {
  for (int i = 0; i < num_tls; i++) {
    if (threads[i] == 0) {
      threads[i] = tid;
      break;
    }
  }
  num_tls++;
}

void omit_tid(pthread_t tid) {
  for (int i = 0; i < num_tls; i++) {
    if (threads[i] == tid) {
      threads[i] = 0;
      break;
    }
  }
  num_tls--;
  if (num_tls == 0) {
    hdestroy();
    hash_initialized = false;
  }
}

/*
 * Lastly, here is a good place to add your externally-callable functions.
 */

int tls_create(unsigned int size) {
  if (num_tls == 0)
    tls_init();

  pthread_t tid = pthread_self();
  if (size == 0 || h_get(tid))
    return -1;

  TLS *lsa = malloc(sizeof(TLS));
  lsa->tid = tid;
  lsa->size = size;
  lsa->num_pages = (size + page_size - 1) / page_size;
  lsa->pages = calloc(lsa->num_pages, sizeof(struct page *));
  for (int i = 0; i < lsa->num_pages; i++) {
    lsa->pages[i] = malloc(sizeof(struct page));
    lsa->pages[i]->address = (size_t)mmap(0, page_size, PROT_NONE,
                                          MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
    lsa->pages[i]->ref_count = 1;
  }

  h_update(tid, lsa);
  register_tid(tid);
  return 0;
}

int tls_destroy() {
  pthread_t tid = pthread_self();
  TLS *lsa = h_get(tid);
  if (lsa == NULL)
    return -1;

  assert(lsa->tid == tid);

  for (int i = 0; i < lsa->num_pages; i++) {
    struct page *p = lsa->pages[i];
    if (--p->ref_count == 0) {
      munmap((void *)(size_t)p->address, page_size);
      free(p);
    }
  }
  free(lsa->pages);
  free(lsa);
  lsa = NULL;
  omit_tid(tid);
  return 0;
}

int tls_read(unsigned int offset, unsigned int length, char *buffer) {
  TLS *lsa = h_get(pthread_self());
  if (lsa == NULL || offset + length > lsa->size)
    return -1;

  assert(lsa->tid == pthread_self());

  for (int i = 0; i < lsa->num_pages; i++)
    tls_unprotect(lsa->pages[i]);

  unsigned int cnt, idx;
  for (cnt = 0, idx = offset; idx < (offset + length); ++cnt, ++idx) {
    struct page *p = lsa->pages[idx / page_size];
    buffer[cnt] = *((char *)(size_t)(p->address + idx % page_size));
  }

  for (int i = 0; i < lsa->num_pages; i++)
    tls_protect(lsa->pages[i]);

  return 0;
}

struct page *create_copy(struct page *p) {
  struct page *copy = malloc(sizeof(struct page));
  copy->address = (size_t)mmap(0, page_size, PROT_WRITE,
                               MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
  copy->ref_count = 1;
  memcpy((void *)(size_t)copy->address, (void *)(size_t)p->address, page_size);
  p->ref_count--;
  tls_protect(p);
  return copy;
}

int tls_write(unsigned int offset, unsigned int length, const char *buffer) {
  TLS *lsa = h_get(pthread_self());
  if (lsa == NULL || offset + length > lsa->size)
    return -1;

  assert(lsa->tid == pthread_self());

  for (int i = 0; i < lsa->num_pages; i++)
    tls_unprotect(lsa->pages[i]);

  unsigned int cnt, idx;
  for (cnt = 0, idx = offset; idx < (offset + length); ++cnt, ++idx) {
    struct page *p = lsa->pages[idx / page_size];
    if (p->ref_count > 1) {
      p = lsa->pages[idx / page_size] = create_copy(p);
    }
    *((char *)(size_t)(p->address + idx % page_size)) = buffer[cnt];
  }

  for (int i = 0; i < lsa->num_pages; i++)
    tls_protect(lsa->pages[i]);

  return 0;
}

TLS *clone(TLS *target) {
  TLS *lsa = malloc(sizeof(TLS));
  lsa->tid = pthread_self();
  lsa->size = target->size;
  lsa->num_pages = target->num_pages;
  lsa->pages = calloc(lsa->num_pages, sizeof(struct page *));
  for (int i = 0; i < lsa->num_pages; i++) {
    lsa->pages[i] = target->pages[i];
    lsa->pages[i]->ref_count++;
  }
  return lsa;
}

int tls_clone(pthread_t tid) {
  pthread_t self_id = pthread_self();
  if (h_get(self_id))
    return -1;

  TLS *target = h_get(tid);
  if (target == NULL)
    return -1;

  assert(target->tid == tid);

  TLS *lsa = clone(target);
  h_update(self_id, lsa);
  register_tid(self_id);
  return 0;
}
