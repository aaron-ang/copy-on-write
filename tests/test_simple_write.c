#include "../tls.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

#define TLS_SIZE 5

static void *write(void *arg) {
  assert(tls_create(TLS_SIZE) == 0);
  assert(tls_write(0, TLS_SIZE, arg) == 0);

  char *read_buffer = malloc(TLS_SIZE);
  assert(tls_read(0, TLS_SIZE, read_buffer) == 0);
  assert(tls_destroy() == 0);
  return read_buffer;
}

int main() {
  pthread_t thread;
  void *pret;

  pthread_create(&thread, 0, &write, "hello");
  pthread_join(thread, &pret);
  assert(strcmp((char *)pret, "hello") == 0);
  free(pret);
}
