#ifndef MULTIPROXY_PROXY
#define MULTIPROXY_PROXY

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <curl/curl.h>

#define PROXY_BUFSIZE 8096

/* Proxy data struct */
typedef struct {
  char *dest;
  char *query;
  pthread_t id;
  int error;
  char error_str[1024];
  char *result;
  size_t size;
} proxy_data;

/* init proxy data */
void proxy_init(proxy_data *data);

/* execute a proxy request */
void *proxy_request(void *);

/* clean up proxy data */
void proxy_cleanup(proxy_data *data);

#endif /* MULTIPROXY_PROXY */
