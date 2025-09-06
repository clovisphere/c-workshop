#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/_types/_ssize_t.h>
#include <sys/errno.h>
#include <sys/fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#define DEFAULT_FILE "index.html"
#define MAX_HEADER_SIZE 256
#define MAX_BUFFER_SIZE 4096


/**
 * write_all - Ensures all bytes in a buffer are written to a file descriptor.
 * @fd: File descriptor to write to (e.g., a socket).
 * @buf: Pointer to the buffer containing data.
 * @len: Number of bytes to write.
 *
 * This function repeatedly calls write() until either all @len bytes
 * have been written or an error occurs. Unlike a single call to write(),
 * this guarantees partial writes are handled correctly.
 *
 * Return: 0 on success, -1 if a write() fails (errno is preserved).
 */
static int write_all(int fd, const char *buf, size_t len) {
    while (len) {
        ssize_t n = write(fd, buf, len);
        if (n < 0) { return -1; }
        buf += n;
        len -= (size_t)n;
    }
    return 0;
}

/**
 * mime_from_path - Guess the MIME type from a file path.
 * @path: Path to a file (e.g., "index.html" or "style.css").
 *
 * This function inspects the file extension and returns a constant
 * string suitable for use as a Content-Type header in HTTP responses.
 * The mapping is minimal and only covers common file types like HTML,
 * CSS, JavaScript, JSON, text, images, etc.
 *
 * Important:
 *   - If the extension is unknown, returns "application/octet-stream".
 *   - The returned pointer is a static string literal; do not free it.
 *
 * Return: MIME type string.
 */
static const char* mime_from_path(const char* path) {
    const char *ext = strrchr(path, '.');
    if (!ext) { return "application/octet-stream"; }
    ext++;

    // minimalist map; extend as you like
    if (!strcmp(ext, "html") || !strcmp(ext, "htm")) { return "text/html; charset=utf-8"; }
    if (!strcmp(ext, "css"))  { return "text/css; charset=utf-8"; }
    if (!strcmp(ext, "js"))   { return "application/javascript; charset=utf-8"; }
    if (!strcmp(ext, "json")) { return "application/json; charset=utf-8"; }
    if (!strcmp(ext, "txt"))  { return "text/plain; charset=utf-8"; }
    if (!strcmp(ext, "svg"))  { return "image/svg+xml"; }
    if (!strcmp(ext, "png"))  { return "image/png"; }
    if (!strcmp(ext, "jpg") || !strcmp(ext, "jpeg")) { return "image/jpeg"; }
    if (!strcmp(ext, "gif")) { return "image/gif"; }
    if (!strcmp(ext, "ico")) { return "image/x-icon"; }

    return "application/octet-stream";
}

/**
 * send_simple_response - Send a complete HTTP response with a small body.
 * @socket: File descriptor for the client socket.
 * @status_line: HTTP status line (e.g., "HTTP/1.1 404 Not Found").
 * @content_type: MIME type string for the response body.
 * @body: Optional body text; may be NULL for empty responses.
 *
 * This function constructs an HTTP response with headers for
 * Content-Type, Content-Length, and Connection: close, followed by
 * the response body (if any). It is mainly used for error responses
 * and simple pages.
 *
 * Notes:
 *   - Content-Length is computed automatically from @body length.
 *   - Body must be a null-terminated string if provided.
 */
static void send_simple_response(
    int socket,
    const char *status_line,
    const char *content_type,
    const char *body
) {

    char header[MAX_HEADER_SIZE];
    size_t body_len = body ? strlen(body) : 0;

    int hdr_len = snprintf(header, sizeof(header),
        "%s\r\n"
        "Content-Type: %s\r\n"
        "Content-Length: %zu\r\n"
        "Connection: close\r\n"
        "\r\n",
        status_line,
        content_type ? content_type : "text/plain; charset=utf-8",
        body_len
    );

    // Best-effort; if header write fails, just bail.
    if (hdr_len < 0 || (size_t)hdr_len >= sizeof(header)) { return; }
    if (write_all(socket, header, (size_t)hdr_len) == -1) { return; }
    if (body_len) { (void)write_all(socket, body, body_len); }
}

/**
 * print_file - Prints the entire contents of a file to stdout.
 * @path: Path to the file to be printed.
 *
 * This function opens the file at @path in read-only mode, retrieves its
 * metadata (size), reads its contents into a buffer, and prints it to stdout.
 * Errors during open, fstat, read, or close are reported to stderr.
 */
void print_file(const char * path) {
    int fd = open(path, O_RDONLY);
    if (fd == -1) {
        perror("Error opening file");
        return;
    }

    struct stat metadata;
    if(fstat(fd, &metadata)) {
        perror("Error getting file metadata");
        close(fd);
        return;
    }

    char * buffer = malloc(metadata.st_size + 1);
    if (!buffer) {
        fprintf(stderr, "Error: memory allocation failed\n");
        close(fd);
        return;
    }

    ssize_t bytes_read = read(fd, buffer, metadata.st_size);
    if (bytes_read == -1) {
        perror("Error reading file");
        free(buffer);
        close(fd);
        return;
    }

    buffer[bytes_read] = '\0';

    printf("\n%s (%zu bytes):\n\n%s\n", path, bytes_read, buffer);

    free(buffer);

    if (close(fd) == -1) {
        perror("Error closing file");
    }
}

/**
 * Extracts the request path from an HTTP GET line and normalizes it.
 *
 * Given a request like:
 *     "GET /blog HTTP/1.1\nHost: example.com"
 *
 * This function will rewrite the buffer in-place so that the path
 * ends with "index.html" and return a pointer to the start of the path:
 *     -> "blog/index.html"
 *
 * Behavior:
 *   - If the path does not end with '/', one is inserted before
 *     appending "index.html".
 *   - If the request is malformed (missing spaces, path, etc.),
 *     returns NULL.
 *
 * Important:
 *   - The input buffer `req` must be writable; this function modifies it.
 *   - Caller should not free or modify the returned pointer separately;
 *     it points inside the original buffer.
 */
char * to_path(char * req) {
    char * start;
    for(start = req; *start != ' '; start++) {
        if (!*start) {
            return nullptr;
        }
    }

    start += 2; // Skip over the space and '/'

    char * end;
    for(end = start; *end != ' '; end++) {
        if (!*end) {
            return nullptr;
        }
    }

    // Ensure there's '/' right before we're about to copy in "index.html"
    if (end[-1] != '/') {
        end[0] = '/';
        end++;
    }

    // Copy in "index.html", overwriting whatever was there in the request string
    memcpy(end, DEFAULT_FILE, strlen(DEFAULT_FILE) + 1);

    return start;
}

/**
 * handle_req - Processes an HTTP request and serves the requested file.
 * @request: The raw HTTP request string (must be writable).
 * @socket: File descriptor for the client socket to write responses to.
 *
 * Sends standards-ish HTTP/1.1 responses with CRLF endings and Content-Length.
 * On success: 200 OK + file bytes.
 * Errors: 400 Bad Request, 404 Not Found, or 500 Internal Server Error.
 */
void handle_req(char * request, int socket) {
    char * path = to_path(request);
    if (!path) {
        send_simple_response(
            socket,
            "HTTP/1.1 400 Bad Request",
            "text/html; charset=utf-8",
            "<h1>400 Bad Request</h1>\n"
        );
        return;
    }

    int fd = open(path, O_RDONLY);
    if (fd == -1) {
        if (errno == ENOENT) {
            send_simple_response(
                socket,
                "HTTP/1.1 404 Not Found",
                "text/html; charset=utf-8",
                "<h1>404 Not Found</h1>\n"
            );
        } else {
            send_simple_response(
                socket,
                "HTTP/1.1 500 Internal Server Error",
                "text/html; charset=utf-8",
                "<h1>500 Internal Server Error</h1>\n"
            );
        }
        return;
    }

    struct stat stats;
    if (fstat(fd, &stats) == -1) {
        close(fd);
        send_simple_response(
            socket,
            "HTTP/1.1 500 Internal Server Error",
            "text/html; charset=utf-8",
            "<h1>500 Internal Server Error</h1>\n"
        );
        return;
    }

    // Build and send success headers with Content-Length
    const char *ctype = mime_from_path(path);
    char header[MAX_HEADER_SIZE];
    int hdr_len = snprintf(header, sizeof(header),
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: %s\r\n"
        "Content-Length: %zu\r\n"
        "Connection: close\r\n"
        "\r\n",
        ctype,
        (size_t)stats.st_size
    );
    if (hdr_len < 0 || (size_t)hdr_len >= sizeof(header)) {
        close(fd);
        send_simple_response(
            socket,
            "HTTP/1.1 500 Internal Server Error",
            "text/html; charset=utf-8",
            "<h1>500 Internal Server Error</h1>\n"
        );
        return;
    }
    if (write_all(socket, header, (size_t)hdr_len) == -1) {
        close(fd);
        return;
    }

    // Stream file body
    char buffer[MAX_BUFFER_SIZE];
    ssize_t bytes_read;
    while ((bytes_read = read(fd, buffer, sizeof(buffer))) > 0) {
        if (write_all(socket, buffer, (size_t)bytes_read) == -1) {
            close(fd);
            return;
        }
    }

    if (bytes_read == -1) {
        // We already sent headers claiming a length; we can't recover cleanly.
        // Best we can do is close; client will see a truncated body.
        // (If you want to be fancier, read the whole file first, then send.)
    }

    if (close(fd) == -1) {
        perror("Error closing file");
    }
}


/**
 * handle_req - Processes an HTTP request and serves the requested file.
 * @request: The raw HTTP request string (must be writable).
 * @socket: File descriptor for the client socket to write responses to.
 *
 * This function:
 *   - Parses the incoming request using `to_path()` to extract a normalized
 *     filesystem path ending in "index.html".
 *   - Opens the requested file and streams its contents to the client socket.
 *   - Writes appropriate HTTP response headers before sending file data.
 *
 * Behavior:
 *   - On malformed requests, writes "400 Bad Request".
 *   - If the requested file does not exist, writes "404 Not Found".
 *   - For other file-related errors (open, fstat, read, write), writes
 *     "500 Internal Server Error".
 *   - On success, writes "200 OK" followed by the file’s contents.
 *
 * Notes:
 *   - The function currently prints error responses to stdout instead of
 *     properly writing them to the socket; this should be replaced for
 *     a real HTTP server implementation.
 *   - Uses blocking I/O: reads the file in chunks (`MAX_BUFFER_SIZE`)
 *     and ensures all bytes are written before proceeding.
 */
// void handle_req(char * request, int socket) {
//     char * path = to_path(request);
//     if(!path) {
//         const char *resp = "HTTP/1.1 400 Bad Request\r\n\r\n";
//         write(socket, resp, strlen(resp));
//     }

//     int fd = open(path, O_RDONLY);
//     if(fd == -1) {
//         if(errno == ENOENT) {
//             // This one is easy to try out in a browser: visit something like
//             // http://localhost:8080/foo (which doesn't exist, so it will 404.)
//             //
//             // Is the output in your terminal different from what you expected?
//             // If so, you can get a clue to what's happening if you run this in a
//             // different terminal window, while watching the output of your C program:
//             //
//             // wget http://localhost:8080/foo
//             const char *resp = "HTTP/1.1 404 Not Found\r\n\r\n";
//             write(socket, resp, strlen(resp));
//         } else {
//             const char *resp = "HTTP/1.1 500 Internal Server Error\r\n\r\n";
//             write(socket, resp, strlen(resp));
//         }
//         return;
//     }

//     struct stat stats;
//     // Populate the `stats` struct with the file's metadata
//     // If it fails (even though the file was open), respond with a 500 error.
//     if(fstat(fd, &stats) == -1) {
//         const char *resp = "HTTP/1.1 500 Internal Server Error\r\n\r\n";
//         write(socket, resp, strlen(resp));
//         close(fd);
//     }

//     // Write the header to the socket ("HTTP/1.1 200 OK\n\n")
//     {
//         const char *success = "HTTP/1.1 200 OK\n\n";
//         size_t bytes_written = 0;
//         size_t bytes_to_write = strlen(success);

//         while(bytes_to_write) {
//             bytes_written = write(socket, success + bytes_written, bytes_to_write);
//             if(bytes_written == -1){
//                 const char *resp = "HTTP/1.1 500 Internal Server Error\r\n\r\n";
//                 write(socket, resp, strlen(resp));
//                 close(fd);
//                 return;
//             }
//             bytes_to_write -= bytes_written;
//         }
//     }

//     {
//         // Read from the file and write to the socket
//         char buffer[MAX_BUFFER_SIZE]; // Buffer for reading file data
//         ssize_t bytes_read;

//         // Loop until we've read the entire file
//         while((bytes_read = read(fd, buffer, sizeof(buffer))) > 0) {
//             ssize_t bytes_written = 0;
//             ssize_t bytes_remaining = bytes_read;

//             // Ensure all bytes are written to the socket
//             while(bytes_remaining > 1) {
//                 ssize_t result = write(socket, buffer + bytes_written, bytes_remaining);

//                 if(result == -1) {
//                     const char *resp = "HTTP/1.1 500 Internal Server Error\r\n\r\n";
//                     write(socket, resp, strlen(resp));
//                     close(fd);
//                     return;
//                 }
//                 bytes_written += result;
//                 bytes_remaining -= result;
//             }
//         }

//         if(bytes_read == -1) {
//             const char *resp = "HTTP/1.1 500 Internal Server Error\r\n\r\n";
//             write(socket, resp, strlen(resp));
//         }
//     }

//     if (close(fd) == -1) {
//         perror("Error closing file");
//     }
// }
