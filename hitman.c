#include <netinet/in.h>    
#include <stdio.h>    
#include <string.h>    
#include <stdlib.h>    
#include <sys/socket.h>    
#include <sys/stat.h>    
#include <sys/types.h>    
#include <unistd.h>    

#include "hitman.h"
#include "sds/sds.h"

#define BACKLOG 10

int on_url(http_parser *parser, const char *at, size_t len) {
    request *request = parser->data;
    request->url = sdsnewlen(at, len);
    return 0;
}

int on_body(http_parser *parser, const char *at, size_t len) {
    request *request = parser->data;
    request->body = sdscatlen(request->body, at, len);
    return 0;
}
    
int on_message_begin(http_parser *parser) {
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

    sds s = sdsnewlen(at, len);
    header->name = header->name == NULL ? s : sdscatsds(header->name, s);
    request->header_state = 1;

    return 0;
}

static int on_header_value(http_parser *parser, const char *at, size_t len) {
    request *request = parser->data;
    header *header = request->headers;

    sds s = sdsnewlen(at, len);
    header->value = header->value == NULL ? s : sdscatsds(header->value, s);
    request->header_state = 2;
    return 0;
}

int on_headers_complete(http_parser *parser) {
    request *request = parser->data;
    request->content_length = parser->content_length;
    request->method = parser->method;
    return 0;
}

int on_message_complete(http_parser *parser) {
    request *request = parser->data;
    request->complete = 1;
    return 0;
}

http_parser_settings parser_settings = {
    .on_message_begin = on_message_begin,
    .on_url = on_url,
    .on_body = on_body,
    .on_header_field = on_header_field,
    .on_header_value = on_header_value,
    .on_headers_complete = on_headers_complete,
    .on_message_complete = on_message_complete,
};

request *handle_client(int fd) {
    char buf[1];
    int ret;

    request *request = malloc(sizeof(struct request));
    memset(request, 0, sizeof(struct request));

    http_parser parser;
    http_parser_init(&parser, HTTP_REQUEST);

    parser.data = request;

    while ((ret = read(fd, buf, sizeof(buf))) > 0) {
        if (http_parser_execute(&parser, &parser_settings, buf, ret) != (size_t) ret) {
            fprintf(stderr, "parse error\n");
            break;
        }
        if (request->complete) break;
    }
    
    return request;
}
    
int main() {    
    int sock, conn;

    socklen_t addrlen;    
    struct sockaddr_in address;    
 
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) == -1){    
        perror("socket");
        exit(1);
    }
    
    address.sin_family = AF_INET;    
    address.sin_addr.s_addr = INADDR_ANY;    
    address.sin_port = htons(3000);    
    
    if (bind(sock, (struct sockaddr *) &address, sizeof(address)) != 0) {    
        perror("bind");
    }
    
    while (1) {    
        if (listen(sock, BACKLOG) < 0) {    
            perror("listen");    
            exit(1);    
        }    
    
        if ((conn = accept(sock, (struct sockaddr *) &address, &addrlen)) < 0) {    
            perror("server: accept");    
            exit(1);    
        }    
        
        request *request = handle_client(conn);

        if (request != NULL) {
            header *header = request->headers;

            while (header != NULL) {
                printf("%s %s\n", header->name, header->value);
                header = header->next;
            }

            write(conn, "<h1>HELLO</1>", 12);
        }

        close(conn);
    }    

    close(sock);
    return 0;    
}
