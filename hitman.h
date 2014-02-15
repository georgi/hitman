#include <netinet/in.h>    
#include <pthread.h>

typedef struct header {
    char *name;
    char *value;
    struct header *next;
} header;

typedef struct request {
    int fd;
    int complete;
    int header_state;
    int method;
    unsigned int content_length;
    char *path;
    header *headers;
    char *body;
} request;

typedef struct handler {
    char *pattern;
    void (*handle)(request *request);
    struct handler *next;
} handler;

typedef struct server {
    struct sockaddr_in address;
    handler *handlers;
} server;

typedef struct server_thread {
    server *server;
    int fd;
    pthread_t thread;
} server_thread;

void add_handler(server *server, char *pattern, void (*handle)(request *request));

void write_status(request *request, int status, char *reason);

void write_header(request *request, char *name, char *value);

void write_body(request *request, char *body);

void free_request(request *request);

int http_serve(server *server);
