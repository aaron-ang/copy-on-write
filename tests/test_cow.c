#include "../tls.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

#define NUM_THREADS 2
#define TLS_SIZE 100

pthread_t threads[NUM_THREADS];

static void *cow(void *arg) {
  int i = *(int *)arg;
  char write_buffer[TLS_SIZE / 2];

  if (i == 0) {
    assert(tls_create(TLS_SIZE) == 0);
    strcpy(write_buffer, "hello");
    assert(tls_write(0, TLS_SIZE / 2, write_buffer) == 0);
  } else {
    assert(tls_clone(threads[0]) == 0);
    strcpy(write_buffer, "world");
    assert(tls_write(0, TLS_SIZE / 2, write_buffer) == 0);
  }

  char *read_buffer = malloc(TLS_SIZE / 2);
  tls_read(0, TLS_SIZE / 2, read_buffer);

  tls_destroy();
  return read_buffer;
}

int main() {
  for (int i = 0; i < NUM_THREADS; i++)
    pthread_create(&threads[i], 0, &cow, &i);

  void *res;
  pthread_join(threads[0], &res);
  assert(strcmp((char *)res, "hello") == 0);
  free(res);

  pthread_join(threads[1], &res);
  assert(strcmp((char *)res, "world") == 0);
  free(res);
}
