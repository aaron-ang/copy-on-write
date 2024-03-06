#include "../tls.h"
#include <assert.h>
#include <string.h>

#define TLS_SIZE 5

static void *read(void *arg) {
  assert(tls_create(TLS_SIZE) == 0);
  char read_buffer[TLS_SIZE];
  assert(tls_read(0, TLS_SIZE, read_buffer) == 0);
  assert(strlen(read_buffer) == 0);
  assert(tls_destroy() == 0);
  return 0;
}

int main() {
  pthread_t thread;
  pthread_create(&thread, 0, &read, "hello");
}
