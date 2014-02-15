#include <netinet/in.h>    
#include <stdlib.h>
#include <unistd.h>
#include "hitman.h"

void handle_bla(request *request) {
    write_status(request, 200, "OK");
    write_header(request, "Content-Type", "text/html");
    write_body(request, "<h1>HELLO</h1>");
}

int main() {
    server server;
    server.handlers = NULL;
    server.address.sin_family = AF_INET;    
    server.address.sin_addr.s_addr = INADDR_ANY;    
    server.address.sin_port = htons(3000);    

    add_handler(&server, "/bla", handle_bla);
    http_serve(&server);

    return 0;
}
    
