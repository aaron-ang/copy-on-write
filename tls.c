#include "tls.h"
#include <assert.h>
#include <search.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>

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

pthread_t threads[MAX_THREAD_COUNT];

int num_tls = 0;

unsigned int page_size;

/*
 * With global data declared, this is a good point to start defining your
 * static helper functions.
 */

static void h_init() {
  if (hcreate(MAX_THREAD_COUNT) == 0) {
    fprintf(stderr, "h_init: could not create hash table\n");
    exit(EXIT_FAILURE);
  }
}

static void hadd(pthread_t tid, TLS *tls) {
  e.key = (char *)tid;
  e.data = tls;
  if (hsearch(e, ENTER) == NULL) {
    fprintf(stderr, "hadd: entry failed\n");
    exit(EXIT_FAILURE);
  }
}

static TLS *hfind(pthread_t tid) {
  e.key = (char *)tid;
  if ((ep = hsearch(e, FIND)) == NULL)
    return NULL;
  return ep->data;
}

static unsigned int get_page_size() {
  // TODO: get page size
  return 4096;
}

static void tls_handle_page_fault(int sig, siginfo_t *si, void *context) {
  // Get base address of page where fault occurred
  unsigned int p_fault = ((unsigned int)si->si_addr) & ~(page_size - 1);
  for (int i = 0; i < num_tls; i++) {
    pthread_t tid = threads[i];
    if (tid == pthread_self() || tid == 0)
      continue;

    TLS *tls = hfind(threads[i]);
    if (tls == NULL)
      continue;
    assert(tls->tid == threads[i]);

    for (int j = 0; j < tls->num_pages; j++) {
      if (tls->pages[j]->address == p_fault)
        pthread_exit(NULL);
    }
  }
  signal(SIGSEGV, SIG_DFL);
  signal(SIGBUS, SIG_DFL);
  raise(sig);
}

static void tls_init() {
  struct sigaction sigact;
  page_size = get_page_size();
  sigemptyset(&sigact.sa_mask);
  sigact.sa_flags = SA_SIGINFO;
  sigact.sa_sigaction = tls_handle_page_fault;
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

/*
 * Lastly, here is a good place to add your externally-callable functions.
 */

int tls_create(unsigned int size) {
  if (num_tls == 0)
    h_init();

  pthread_t tid = pthread_self();
  if (size == 0 || hfind(tid))
    return -1;

  tls_init();

  TLS *lsa = malloc(sizeof(TLS));
  lsa->tid = tid;
  lsa->size = size;
  lsa->num_pages = size / page_size;
  lsa->pages = calloc(lsa->num_pages, sizeof(struct page *));
  for (int i = 0; i < lsa->num_pages; i++) {
    lsa->pages[i] = malloc(sizeof(struct page));
    lsa->pages[i]->address = (unsigned int)mmap(
        0, page_size, PROT_NONE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
    lsa->pages[i]->ref_count = 1;
  }

  hadd(tid, lsa);
  threads[num_tls] = tid;
  num_tls++;
  return 0;
}

int tls_destroy() {
  pthread_t tid = pthread_self();
  TLS *lsa = hfind(tid);
  if (lsa == NULL)
    return -1;
  assert(lsa->tid == tid);

  for (int i = 0; i < lsa->num_pages; i++) {
    struct page *p = lsa->pages[i];
    if (--p->ref_count == 0) {
      munmap((void *)p->address, page_size);
      free(p);
    } else {
      // TODO: handle shared page
    }
  }
  free(lsa);
  lsa = NULL;

  for (int i = 0; i < num_tls; i++) {
    if (threads[i] == tid) {
      threads[i] = 0;
      break;
    }
  }
  if (--num_tls == 0)
    hdestroy();

  return 0;
}

int tls_read(unsigned int offset, unsigned int length, char *buffer) {
  TLS *lsa = hfind(pthread_self());
  if (lsa == NULL || offset + length > lsa->size)
    return -1;
  assert(lsa->tid == pthread_self());

  for (int i = 0; i < lsa->num_pages; i++)
    tls_unprotect(lsa->pages[i]);

  unsigned int cnt, idx;
  for (cnt = 0, idx = offset; idx < (offset + length); ++cnt, ++idx) {
    struct page *p = lsa->pages[idx / page_size];
    buffer[cnt] = *((char *)(p->address + idx % page_size));
  }

  for (int i = 0; i < lsa->num_pages; i++)
    tls_protect(lsa->pages[i]);

  return 0;
}

int tls_write(unsigned int offset, unsigned int length, const char *buffer) {
  TLS *lsa = hfind(pthread_self());
  if (lsa == NULL || offset + length > lsa->size)
    return -1;
  assert(lsa->tid == pthread_self());

  for (int i = 0; i < lsa->num_pages; i++)
    tls_unprotect(lsa->pages[i]);

  // TODO: perform write operation

  for (int i = 0; i < lsa->num_pages; i++)
    tls_protect(lsa->pages[i]);

  return 0;
}

int tls_clone(pthread_t tid) {
  if (hfind(pthread_self()))
    return -1;

  TLS *target = hfind(tid);
  if (target == NULL)
    return -1;
  assert(target->tid == tid);

  TLS *lsa = malloc(sizeof(TLS));
  lsa->tid = pthread_self();
  lsa->size = target->size;
  lsa->num_pages = target->num_pages;
  lsa->pages = calloc(lsa->num_pages, sizeof(struct page *));
  for (int i = 0; i < lsa->num_pages; i++) {
    lsa->pages[i] = target->pages[i];
    lsa->pages[i]->ref_count++;
  }

  hadd(pthread_self(), lsa);
  threads[num_tls] = pthread_self();
  num_tls++;

  return 0;
}
