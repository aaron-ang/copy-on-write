#include "../tls.h"
#include <assert.h>
#include <string.h>
#include <unistd.h>

#define TLS_SIZE 5

pthread_t thread_create, thread_cow;
char write_buffer[TLS_SIZE], read_buffer[TLS_SIZE];

static void *create() {
  const char *str = "hello";
  assert(tls_create(TLS_SIZE) == 0);
  strcpy(write_buffer, str);
  assert(tls_write(0, TLS_SIZE, write_buffer) == 0);

  sleep(1); // allow thread_cow to clone and write

  assert(tls_read(0, TLS_SIZE, read_buffer) == 0);
  assert(strcmp(read_buffer, str) == 0);
  assert(tls_destroy() == 0);
  return 0;
}

static void *cow(void *arg) {
  const char *str = "world";
  assert(tls_clone(thread_create) == 0);
  strcpy(write_buffer, str);
  assert(tls_write(0, TLS_SIZE, write_buffer) == 0); // COW
  assert(tls_read(0, TLS_SIZE, read_buffer) == 0);
  assert(strcmp(read_buffer, str) == 0);
  assert(tls_destroy() == 0);
  return 0;
}

int main() {
  pthread_create(&thread_create, 0, &create, 0);
  pthread_create(&thread_cow, 0, &cow, 0);
}
