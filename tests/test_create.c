#include "../tls.h"
#include <assert.h>

#define TLS_SIZE 5

static void *create() {
  assert(tls_create(TLS_SIZE) == 0);
  assert(tls_destroy() == 0);
  return 0;
}

int main() {
  pthread_t thread;
  pthread_create(&thread, 0, &create, 0);
  pthread_join(thread, NULL);
}
