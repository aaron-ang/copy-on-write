#ifndef _XOPEN_SOURCE
#define _XOPEN_SOURCE 600
#endif

#ifndef TLS_H_
#define TLS_H_
#include <pthread.h>

int tls_create(unsigned int size);
int tls_destroy();
int tls_read(unsigned int offset, unsigned int length, char *buffer);
int tls_write(unsigned int offset, unsigned int length, const char *buffer);
int tls_clone(pthread_t tid);

#endif /* TLS_H_ */
