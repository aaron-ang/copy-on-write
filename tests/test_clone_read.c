#include "../tls.h"
#include <assert.h>
#include <string.h>
#include <unistd.h>

#define TLS_SIZE 5

pthread_t thread_create, thread_clone;

static void *create() {
  assert(tls_create(TLS_SIZE) == 0);
  sleep(1); // wait for thread_clone to clone and read
  assert(tls_destroy() == 0);
  return 0;
}

static void *clone_read() {
  char read_buffer[TLS_SIZE];
  assert(tls_clone(thread_create) == 0);
  assert(tls_read(0, TLS_SIZE, read_buffer) == 0);
  assert(strlen(read_buffer) == 0);
  assert(tls_destroy() == 0);
  return 0;
}

int main() {
  pthread_create(&thread_create, 0, &create, 0);
  pthread_create(&thread_clone, 0, &clone_read, 0);
}
