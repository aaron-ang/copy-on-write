#include "../tls.h"
#include <assert.h>
#include <unistd.h>

#define TLS_SIZE 5

pthread_t thread_create, thread_clone;

static void *create() {
  assert(tls_create(TLS_SIZE) == 0);
  sleep(1); // allow thread_clone to call tls_clone
  assert(tls_destroy() == 0);
  return 0;
}

static void *clone() {
  assert(tls_clone(thread_create) == 0);
  assert(tls_destroy() == 0);
  return 0;
}

int main() {
  pthread_create(&thread_create, 0, &create, 0);
  pthread_create(&thread_clone, 0, &clone, 0);
}
