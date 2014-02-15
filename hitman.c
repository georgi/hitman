#include <netinet/in.h>    
#include <stdio.h>    
#include <string.h>    
#include <stdlib.h>    
#include <sys/socket.h>    
#include <sys/stat.h>    
#include <sys/types.h>    
#include <unistd.h>    
#include <errno.h>

#include "hitman.h"
#include "http-parser/http_parser.h"
#include "sds/sds.h"

#define BACKLOG 5

void add_handler(server *server, char *pattern, void (*handle)(request *request)) {
    handler *handler = malloc(sizeof(handler));
    handler->handle = handle;
    handler->pattern = sdsnew(pattern);
    handler->next = server->handlers;
    server->handlers = handler;
}

void write_header(request *request, char *name, char *value) {
    sds s = sdsempty();
    s = sdscatprintf(s, "%s: %s\r\n", name, value);
    write(request->fd, s, sdslen(s));
    sdsfree(s);
}

void write_status(request *request, int status, char *reason) {
    sds s = sdsempty();
    s = sdscatprintf(s, "HTTP/1.1 %d %s\r\n", status, reason);
    write(request->fd, s, sdslen(s));
}

void write_body(request *request, char *body) {
    write(request->fd, "\r\n", 2);
    write(request->fd, body, strlen(body));
}

static int on_url(http_parser *parser, const char *at, size_t len) {
    request *request = parser->data;
    request->path = sdsnewlen(at, len);
    return 0;
}

static int on_body(http_parser *parser, const char *at, size_t len) {
    request *request = parser->data;
    request->body = sdscatlen(request->body, at, len);
    return 0;
}
    
static int on_message_begin(http_parser *parser) {
    request *request = parser->data;
    request->body = sdsempty();
    return 0;
}

static int on_header_field(http_parser *parser, const char *at, size_t len) {
    request *request = parser->data;
    header *header;

    if (request->header_state != 1) {
        header = malloc(sizeof(struct header));
        header->name = NULL;
        header->value = NULL;
        header->next = request->headers;
        request->headers = header;
    } else {
        header = request->headers;
    }

    header->name = sdscatlen(header->name == NULL ? sdsempty() : header->name, at, len);
    request->header_state = 1;

    return 0;
}

static int on_header_value(http_parser *parser, const char *at, size_t len) {
    request *request = parser->data;
    header *header = request->headers;
    header->value = sdscatlen(header->value == NULL ? sdsempty() : header->value, at, len);
    request->header_state = 2;
    return 0;
}

static int on_headers_complete(http_parser *parser) {
    request *request = parser->data;
    request->content_length = parser->content_length;
    request->method = parser->method;
    return 0;
}

static int on_message_complete(http_parser *parser) {
    request *request = parser->data;
    request->complete = 1;
    return 0;
}

static http_parser_settings parser_settings = {
    .on_message_begin = on_message_begin,
    .on_url = on_url,
    .on_body = on_body,
    .on_header_field = on_header_field,
    .on_header_value = on_header_value,
    .on_headers_complete = on_headers_complete,
    .on_message_complete = on_message_complete,
};

void free_request(request *request) {
    header *header = request->headers;

    while (header != NULL) {
        struct header *prev = header;
        sdsfree(header->name);
        sdsfree(header->value);
        header = header->next;
        free(prev);
    }
    
    sdsfree(request->body);
    sdsfree(request->path);

    free(request);
}

int parse_request(request *request) {
    char buf[4096];
    int ret;

    http_parser parser;
    http_parser_init(&parser, HTTP_REQUEST);

    parser.data = request;

    while ((ret = read(request->fd, buf, sizeof(buf))) > 0) {
        if (http_parser_execute(&parser, &parser_settings, buf, ret) != (size_t) ret) {
            return -1;
        }
        if (request->complete) break;
    }
    
    return 0;
}
        
static void handle_request(int fd, handler *handlers) {
    request *request = malloc(sizeof(struct request));
    memset(request, 0, sizeof(struct request));
    request->fd = fd;

    if (parse_request(request) == 0) {
        handler *handler = handlers;
        while (handler != NULL) {
            if (strncmp(handler->pattern, request->path, strlen(handler->pattern)) == 0) {
                handler->handle(request);
                goto cleanup;
            }
            handler = handler->next;
        }
        write_status(request, 500, "INTERNAL SERVER ERROR");
    } else {
        write_status(request, 400, "BAD REQUEST");
    }

 cleanup:
    close(fd);
    free_request(request);
}

static void *run_thead(void *ptr) {
    server_thread *thread = ptr;
    pthread_detach(thread->thread);
    handle_request(thread->fd, thread->server->handlers);
    free(thread);
    return NULL;
}
    
int http_serve(server *server) {    
    int sock, fd;
 
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) == -1){    
        perror("could not open socket");
        return -1;
    }
    
    if (bind(sock, (struct sockaddr *) &server->address, sizeof(server->address)) != 0) {    
        perror("could not bind address");
        return -1;
    }

    if (listen(sock, BACKLOG) < 0) {    
        perror("could not listen on socket");    
        return -1;
    }    
    
    while (1) {    
        socklen_t addrlen;    

        if ((fd = accept(sock, (struct sockaddr *) &server->address, &addrlen)) == 0) {    
            perror("could not accept socket");    
            exit(1);
        }

        server_thread *thread = malloc(sizeof(server_thread));
        thread->fd = fd;
        thread->server = server;

        if (pthread_create(&thread->thread, NULL, run_thead, (void *) thread) != 0) {
            perror("could not create thread");    
            exit(1);
        }
    }    

    close(sock);

    return 0;    
}
