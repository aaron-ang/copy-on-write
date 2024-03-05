#include "tls.h"
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
  unsigned int size;     /* size in bytes */
  unsigned int page_num; /* number of pages */
  struct page **pages;   /* array of pointers to pages */
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

int num_tls = 0;

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
  e.data = (void *)tls;
  if (hsearch(e, ENTER) == NULL) {
    fprintf(stderr, "hadd: entry failed\n");
    exit(EXIT_FAILURE);
  }
}

static TLS *hfind(pthread_t tid) {
  e.key = (char *)tid;
  if ((ep = hsearch(e, FIND)) == NULL)
    return NULL;
  return (TLS *)ep->data;
}

static void tls_handle_page_fault(int sig, siginfo_t *si, void *context) {
  TLS *lsa = hfind(pthread_self());
  unsigned int p_fault = ((unsigned int)si->si_addr) & ~(lsa->size - 1);
  // TODO: brute force scan through all allocated TLS regions
  // exit just the current thread if faulting page is not found
  // pthread_exit(NULL);
  signal(SIGSEGV, SIG_DFL);
  signal(SIGBUS, SIG_DFL);
  raise(sig);
}

static void tls_init() {
  struct sigaction sigact;
  // page_size = getpagesize();
  sigemptyset(&sigact.sa_mask);
  sigact.sa_flags = SA_SIGINFO;
  sigact.sa_sigaction = tls_handle_page_fault;
  sigaction(SIGBUS, &sigact, NULL);
  sigaction(SIGSEGV, &sigact, NULL);
}

static void tls_protect(struct page *p) {
  // if (mprotect((void *)p->address, ???->size, ???) != 0) {
  //   fprintf(stderr, "tls_protect: could not protect page\n");
  //   exit(EXIT_FAILURE);
  // }
}

static void tls_unprotect(struct page *p) {
  // if (mprotect((void *) p->address, page_size, ???)) {
  //   fprintf(stderr, "tls_unprotect: could not unprotect page\n");
  //   exit(EXIT_FAILURE);
  // }
}

/*
 * Lastly, here is a good place to add your externally-callable functions.
 */

int tls_create(unsigned int size) {
  if (num_tls == 0)
    h_init();

  TLS *lsa;
  if ((lsa = hfind(pthread_self())) && lsa->size > 0)
    return -1;

  if (lsa == NULL) {
    // TODO: allocate TLS
  }

  hadd(pthread_self(), NULL);

  num_tls++;
  return 0;
}

int tls_destroy() {
  e.key = (char *)pthread_self();
  if ((ep = hsearch(e, FIND)) == NULL)
    return -1;

  // TODO: clean up all pages

  free(ep->data);
  ep->data = NULL;

  if (--num_tls == 0)
    hdestroy();

  return 0;
}

int tls_read(unsigned int offset, unsigned int length, char *buffer) {
  return 0;
}

int tls_write(unsigned int offset, unsigned int length, const char *buffer) {
  return 0;
}

int tls_clone(pthread_t tid) { return 0; }
