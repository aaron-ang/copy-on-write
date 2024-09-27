# Copy on Write

This project implements a library that provides protected memory regions for threads using copy-on-write, which can be used for thread local storage (TLS).

## Definitions
* **Thread-local storage (TLS)**: each thread is reserved a part of memory. Other threads will not successfully access that
memory unless a clone operation explicitly shares it. Each thread has its own Local Storage Area (LSA).

* **Copy-on-write (COW)**: cloned data references the same location in memory.
Once there is any write to the cloned data, a copy of the data is used for the write.
Furthermore, subsequent reads from the writer will see the modified data, and references by other readers will still reference the unmodified memory.

## Data Structures
```C
#define MAX_THREAD_COUNT 128

typedef struct thread_local_storage {
  pthread_t tid;
  unsigned int size;      /* size in bytes */
  unsigned int num_pages;
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

static unsigned int page_size;

static struct tid_tls_pair tid_tls_pairs[MAX_THREAD_COUNT];
```
