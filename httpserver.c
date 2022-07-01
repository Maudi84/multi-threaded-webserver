#include <err.h>
#include <fcntl.h>
#include <inttypes.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <pthread.h>

#include "io.h"
#include "util.h"
#include "threadpool.h"

#define OPTIONS              "t:l:"
#define BUF_SIZE             4096
#define DEFAULT_THREAD_COUNT 4

static FILE *logfile;
#define LOG(...) fprintf(logfile, __VA_ARGS__);
struct thread_pool *worker_pool = NULL;

// Converts a string to an 16 bits unsigned integer.
// Returns 0 if the string is malformed or out of the range.
static size_t strtouint16(char number[]) {
    char *last;
    long num = strtol(number, &last, 10);
    if (num <= 0 || num > UINT16_MAX || *last != '\0') {
        return 0;
    }
    return num;
}

// Creates a socket for listening for connections.
// Closes the program and prints an error message on error.
static int create_listen_socket(uint16_t port) {
    struct sockaddr_in addr;
    int listenfd = socket(AF_INET, SOCK_STREAM, 0);
    if (listenfd < 0) {
        err(EXIT_FAILURE, "socket error");
    }
    memset(&addr, 0, sizeof addr);
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htons(INADDR_ANY);
    addr.sin_port = htons(port);
    if (bind(listenfd, (struct sockaddr *) &addr, sizeof addr) < 0) {
        err(EXIT_FAILURE, "bind error");
    }
    if (listen(listenfd, 128) < 0) {
        err(EXIT_FAILURE, "listen error");
    }
    return listenfd;
}

void *handle_connection(void *socket) {
    //	request buffer
    char request_buffer[2049];
    memset(request_buffer, '\0', 2049);
    //	header buffer
    char header_buffer[500];
    memset(header_buffer, 0, 500);
    // buffer used for transfering bytes from file to socket
    uint8_t file_buffer[4096];
    memset(file_buffer, 0, 4096);
    // buffer for log entry
    char log_entry[250];
    memset(log_entry, 0, 250);
    // holds method name
    char method[10];
    memset(method, 0, 10);
    // holds uri
    char uri[25];
    memset(uri, 0, 25);
    // holds uri
    char version[10];
    memset(uri, 0, 10);
    //	holds request id
    char request_id[25];
    memset(request_id, 0, 25);
    request_id[0] = '0';
    request_id[1] = '\0';
    // process flags
    int process_level = 0;
    int method_type = 0;
    int path_check = 0;
    // byte checkers
    size_t content_length = 0;
    size_t bytes_read = 0;
    size_t total_bytes_read = 0;
    int cursor_pos = 0;
    //file headers
    int out_file = 0;
    int in_file = 0;
    int connfd = *((int *) socket);
    // Read from connfd until EOF or error.

    while (total_bytes_read < 2048) {
        bytes_read = read(connfd, request_buffer + total_bytes_read, 2048 - total_bytes_read);
        total_bytes_read = total_bytes_read + bytes_read;
        if (!bytes_read) {
            break;
        }
    }
    //	verifying request
    cursor_pos = verify_request(request_buffer, method, uri, version, total_bytes_read);
    if (cursor_pos > 0) {

        process_level = 1;
    } else {
        req_error_response(cursor_pos, connfd);
    }

    // verifying header
    if (process_level > 0) {
        cursor_pos = grab_header(request_buffer, header_buffer, total_bytes_read, cursor_pos);
        if (cursor_pos > 0) {
            process_level = 2;
        } else {
            req_error_response(cursor_pos, connfd);
        }
    }
    //	verifying method type
    if (process_level > 1) {
        // looking for request id
        get_request_id(header_buffer, request_id);
        method_type = check_method(method);
        if (method_type > 0) {
            process_level = 3;
        } else {
            not_implemented_error(connfd);
        }
    }

    //`verifies file path
    if (process_level > 2) {
        path_check = check_path(uri);
        if (path_check > 0) {
            process_level = 4;
        } else {
            if (path_check == 0 && method_type == 2) {
                process_level = 4;
            } else {
                switch (path_check) {
                case 0:
                    log_request(log_entry, method, uri, "404", request_id);
                    path_error_response(path_check, connfd, log_entry, logfile);
                    break;
                case -1:
                    log_request(log_entry, method, uri, "403", request_id);
                    path_error_response(path_check, connfd, log_entry, logfile);
                    break;
                case -2:
                    log_request(log_entry, method, uri, "500", request_id);
                    path_error_response(path_check, connfd, log_entry, logfile);
                    break;
                }
            }
        }
    }
    // processing the method
    if (process_level > 3) {
        content_length = get_content_length(header_buffer);
        switch (method_type) {
        case 1:
            log_request(log_entry, method, uri, "200", request_id);
            out_file = open(uri + 1, O_RDONLY);
            get_request_response(out_file, connfd, file_buffer, uri, log_entry, logfile);
            close(out_file);
            break;
        case 2:
            in_file = open(uri + 1, O_WRONLY | O_CREAT | O_TRUNC, 0666);
            if (path_check > 0) {
                log_request(log_entry, method, uri, "200", request_id);
                put_request_response(connfd, in_file, content_length, file_buffer, request_buffer,
                    log_entry, cursor_pos, total_bytes_read, logfile);
                close(in_file);
                break;
            } else {
                log_request(log_entry, method, uri, "201", request_id);
                put_create_response(connfd, in_file, content_length, file_buffer, request_buffer,
                    log_entry, cursor_pos, total_bytes_read, logfile);
                close(in_file);
                break;
            }
        case 3:
            in_file = open(uri + 1, O_WRONLY | O_APPEND);
            log_request(log_entry, method, uri, "200", request_id);
            put_request_response(connfd, in_file, content_length, file_buffer, request_buffer,
                log_entry, cursor_pos, total_bytes_read, logfile);

            close(in_file);
            break;
        }
    }
    // we're all done, closing
    free(socket);
    close(connfd);
    return NULL;
}

static void sigterm_handler(int sig) {
    if (sig == SIGTERM || sig == SIGINT) {
        warnx("received SIGTERM");
        empty_pool(worker_pool);
        fclose(logfile);
        exit(EXIT_SUCCESS);
    }
}

static void usage(char *exec) {
    fprintf(stderr, "usage: %s [-t threads] [-l logfile] <port>\n", exec);
}

int main(int argc, char *argv[]) {

    int opt = 0;
    int threads = DEFAULT_THREAD_COUNT;
    logfile = stderr;

    while ((opt = getopt(argc, argv, OPTIONS)) != -1) {
        switch (opt) {
        case 't':
            threads = strtol(optarg, NULL, 10);
            if (threads <= 0) {
                errx(EXIT_FAILURE, "bad number of threads");
            }
            break;
        case 'l':
            logfile = fopen(optarg, "w");
            if (!logfile) {
                errx(EXIT_FAILURE, "bad logfile");
            }
            break;
        default: usage(argv[0]); return EXIT_FAILURE;
        }
    }

    if (optind >= argc) {
        warnx("wrong number of arguments");
        usage(argv[0]);
        return EXIT_FAILURE;
    }

    uint16_t port = strtouint16(argv[optind]);

    if (port == 0) {
        errx(EXIT_FAILURE, "bad port number: %s", argv[1]);
    }
    signal(SIGPIPE, SIG_IGN);
    signal(SIGTERM, sigterm_handler);
    signal(SIGINT, sigterm_handler);

    int listenfd = create_listen_socket(port);

    worker_pool = fill_pool(threads);

    for (;;) {
        signal(SIGTERM, sigterm_handler);
        int connfd = accept(listenfd, NULL, NULL);
        if (connfd < 0) {
            warn("accept error");
            continue;
        }
        int *connection = malloc(sizeof(int));
        *connection = connfd;

        hire_worker(worker_pool, connection);
    }
    empty_pool(worker_pool);
    fclose(logfile);
    exit(EXIT_SUCCESS);
    return EXIT_SUCCESS;
}
