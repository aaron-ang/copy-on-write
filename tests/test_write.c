#include "../tls.h"
#include <assert.h>

#define TLS_SIZE 5

static void *write(void *arg) {
  assert(tls_create(TLS_SIZE) == 0);
  assert(tls_write(0, TLS_SIZE, arg) == 0);
  assert(tls_destroy() == 0);
  return 0;
}

int main() {
  pthread_t thread;
  pthread_create(&thread, 0, &write, "hello");
}
