#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/_endian.h>
#include <sys/_types/_socklen_t.h>
#include <sys/_types/_ssize_t.h>
#include <sys/socket.h>
#include <unistd.h>

#include "lib.c"

#define PORT 8080
#define MAX_REQUEST_BYTES 32768

// starts program execution
int main() {
    int socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if(socket_fd == -1) {
        perror("Error opening socket");
        return EXIT_FAILURE;
    }
    // Prevent `Address in use` errors when restarting the server
    int opt = 1;
    if(setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1) {
        perror("Error setting socket options");
        return EXIT_FAILURE;
    }

    struct sockaddr_in address; // IPv4 address
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    if(bind(socket_fd, (struct sockaddr *)&address, sizeof(address))) {
        perror("Error binding address");
        return EXIT_FAILURE;
    }

    if(listen(socket_fd, 4) == -1){
        perror("Error listening");
        return EXIT_FAILURE;
    }

    printf("\nListening on port %d\n", PORT);

    char req[MAX_REQUEST_BYTES + 1];
    int addrlen = sizeof(address);

    // Loop forever to keep processing new connections
    while(1) {
        // Block until we get a connection on the socket
        int req_socket_fd = accept(socket_fd, (struct sockaddr *)&address, (socklen_t *)&addrlen);
        if(req_socket_fd >= 0) {
            // Read all the bytes from the socket into the buffer
            ssize_t bytes_read = read(req_socket_fd, req, MAX_REQUEST_BYTES);

            if(bytes_read < MAX_REQUEST_BYTES) {
                req[bytes_read] = '\0';
                // Parse the URL and method out of the HTTP request
                handle_req(req, req_socket_fd);
            } else {
                // The request was larger than the maximum size we support!
                const char *resp = "HTTP/1.1 413 Content Too Large\r\n\r\n";
                write(req_socket_fd, resp, strlen(resp));
            }

            if(close(req_socket_fd) == -1) {
                perror("Error closing connection");
            }
        }
        // Continue listening for other connections even if accept fails
    }
    return EXIT_SUCCESS;
}
