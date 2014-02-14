#include "http-parser/http_parser.h"

typedef struct header {
    char *name;
    char *value;
    struct header *next;
} header;

typedef struct request {
    int fd;
    int complete;
    int header_state;
    enum http_method method;
    unsigned int content_length;
    char *path;
    char *url;
    header *headers;
    char *body;
} request;
