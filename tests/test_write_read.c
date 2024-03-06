#include "../tls.h"
#include <assert.h>
#include <string.h>

#define TLS_SIZE 5

static void *write_read(void *arg) {
  assert(tls_create(TLS_SIZE) == 0);
  assert(tls_write(0, TLS_SIZE, arg) == 0);

  char read_buffer[TLS_SIZE];
  assert(tls_read(0, TLS_SIZE, read_buffer) == 0);
  assert(strcmp(read_buffer, arg) == 0);
  assert(tls_destroy() == 0);
  return 0;
}

int main() {
  pthread_t thread;
  pthread_create(&thread, 0, &write_read, "hello");
  pthread_join(thread, NULL);
}
