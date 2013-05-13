#ifndef MULTIPROXY_HANDLERS
#define MULTIPROXY_HANDLERS

void handler_init();

void multipart_handler(int fd, char *uri, char *query);
void a_handler(int fd, char *uri, char *query);
void b_handler(int fd, char *uri, char *query);
void c_handler(int fd, char *uri, char *query);
void send_resturaunt(int fd, char *uri, char *resturaunt);

/* Static table and typedefs for our handlers */
typedef void (*handler_fp)(int fd, char *uri, char *query);

#endif // MULTIPROXY_HANDLERS
