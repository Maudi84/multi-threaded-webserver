// Some code used from 13S Compression PA
#include <sys/file.h> //	flock()

#include <stdio.h> //	sprintf()
#include <pthread.h> //	pthread_mutex_t
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <sys/stat.h>
#include <unistd.h>

#include "util.h"
// Begin user defined headers

#include "io.h"

pthread_mutex_t loglock = PTHREAD_MUTEX_INITIALIZER;

char *empty_response_200 = "HTTP/1.1 200 OK\r\nContent-Length: 0\r\n\r\n";

char *stub_response_200 = "HTTP/1.1 200 OK\r\nContent-Length: ";

char *generic_response_200 = "HTTP/1.1 200 OK\r\nContent-Length: 3\r\n\r\n";

char *generic_response_201 = "HTTP/1.1 201 Created\r\nContent-Length: 8\r\n\r\n";

char *response_400 = "HTTP/1.1 400 Bad Request\r\nContent-Length: 12\r\n\r\n";

char *response_403 = "HTTP/1.1 403 Forbidden\r\nContent-Length: 10\r\n\r\n";

char *response_404 = "HTTP/1.1 404 Not Found\r\nContent-Length: 10\r\n\r\n";

char *response_500 = "HTTP/1.1 500 Internal Server Error\r\nContent-Length: 22\r\n\r\n";

char *response_501 = "HTTP/1.1 501 Not Implemented\r\nContent-Length: 16\r\n\r\n";

int read_bytes(int infile, uint8_t *buf,
    int to_read) { // contiously runs read command until buffer full

    // or no more bytes
    int read_bytes = 0; // tells us how much we got from read
    int total_bytes = 0; // tells us the total amount we got from read
    do { // run this loop at least once
        read_bytes = read(infile, buf + total_bytes,
            (to_read - total_bytes)); // read in the bytes
        if (read_bytes < 0) {
            return -1;
        }

        total_bytes += read_bytes; // add it to total bytes

    } while (read_bytes > 0 && total_bytes != to_read); // as long as read gives
        // us something and total
        // read does not equal
        // total needed
    return total_bytes;
}

int write_bytes(int outfile, char *buf,
    int to_write) { // same as above but for writing bytes
    int written_bytes = 0;
    int total_bytes = 0;
    do {
        written_bytes = write(outfile, buf + total_bytes, (to_write - total_bytes));
        if (written_bytes < 0) {
            return -1;
        }
        total_bytes += written_bytes;

    } while (written_bytes > 0 && total_bytes != to_write);
    return total_bytes;
}

int write_bytes_u(int outfile, uint8_t *buf,
    int to_write) { // same as above but for writing bytes
    int written_bytes = 0;
    int total_bytes = 0;
    do {
        written_bytes = write(outfile, buf + total_bytes, (to_write - total_bytes));
        if (written_bytes < 0) {
            return -1;
        }
        total_bytes += written_bytes;

    } while (written_bytes > 0 && total_bytes != to_write);
    return total_bytes;
}

// Begin functions
void bad_request_error(int socket) {
    write_bytes(socket, response_400, 48);
    write_bytes(socket, "Bad Request\n", 12);
}

void forbidden_path_error(int socket) {
    write_bytes(socket, response_403, 46);
    write_bytes(socket, "Forbidden\n", 10);
}

void not_found_error(int socket) {
    write_bytes(socket, response_404, 46);
    write_bytes(socket, "Not Found\n", 10);
}

void internal_server_error(int socket) {
    write_bytes(socket, response_500, 58);
    write_bytes(socket, "Internal Server Error\n", 22);
}

void not_implemented_error(int socket) {
    write_bytes(socket, response_501, 52);
    write_bytes(socket, "Not Implemented\n", 16);
}

void get_empty_response(int socket) {
    write_bytes(socket, empty_response_200, 38);
}

void put_200_response(int socket) {
    write_bytes(socket, generic_response_200, 38);
    write_bytes(socket, "OK\n", 3);
}

void put_201_response(int socket) {
    write_bytes(socket, generic_response_201, 43);
    write_bytes(socket, "Created\n", 8);
}

int write_file_buffer(int socket, int out_file, int file_size, uint8_t *file_buffer) {

    int bytes_written = 0;
    int bytes_read = 0;
    int total_bytes = 0;

    while (total_bytes < file_size) {
        if (file_size - total_bytes < 4096) {
            bytes_read = read_bytes(out_file, file_buffer, file_size - total_bytes);
            if (!bytes_read) {
                return total_bytes;
            }
        } else {
            bytes_read = read_bytes(out_file, file_buffer, 4096);
            if (!bytes_read) {
                return total_bytes;
            }
        }
        if (bytes_read < 0) {
            return -1;
        }
        bytes_written = write_bytes_u(socket, file_buffer, bytes_read);
        if (bytes_written < 0) {
            return -1;
        }
        total_bytes = total_bytes + bytes_written;
    }
    return total_bytes;
}

int read_file_buffer(int socket, int in_file, size_t content_length, uint8_t *file_buffer,
    char *socket_buffer, int cursor_pos, int total_buf_size) {
    int bytes_left = content_length;
    int bytes_read = 0;
    int bytes_wrote = 0;
    int total_bytes = 0;
    int flush_count = 0;

    flush_count = flush_header(socket_buffer, file_buffer, cursor_pos, total_buf_size);
    if (flush_count) {
        if (bytes_left <= flush_count) {
            bytes_wrote = write_bytes_u(in_file, file_buffer, bytes_left);
            return bytes_wrote;
        }
        if (bytes_left > 4096) {
            bytes_read = read(socket, file_buffer + flush_count, (4096 - flush_count));
            if (bytes_read < 1) {
                bytes_wrote = write_bytes_u(in_file, file_buffer, flush_count);
                return bytes_wrote;
            }
            bytes_wrote = write_bytes_u(in_file, file_buffer, bytes_read + flush_count);
            bytes_left = bytes_left - bytes_wrote;
            total_bytes = total_bytes + bytes_wrote;
        } else {
            bytes_read = read(socket, file_buffer + flush_count, bytes_left - flush_count);
            if (bytes_read < 1) {
                bytes_wrote = write_bytes_u(in_file, file_buffer, flush_count);
                return bytes_wrote;
            }
            bytes_wrote = write_bytes_u(in_file, file_buffer, bytes_read + flush_count);
            return bytes_wrote;
        }
    }
    while (bytes_left > 0) {
        if (bytes_left > 4096) {
            bytes_read = read(socket, file_buffer, 4096);
            if (bytes_read < 1) {
                return total_bytes;
            }
            bytes_wrote = write_bytes_u(in_file, file_buffer, bytes_read);
            bytes_left = bytes_left - bytes_wrote;
            total_bytes = total_bytes + bytes_wrote;
        } else {
            bytes_read = read(socket, file_buffer, bytes_left);
            if (bytes_read < 1) {
                return total_bytes;
            }
            bytes_wrote = write_bytes_u(in_file, file_buffer, bytes_read);
            bytes_left = bytes_left - bytes_wrote;
            total_bytes = total_bytes + bytes_wrote;
            return total_bytes;
        }
    }
    return total_bytes;
}

void put_create_response(int socket, int in_file, size_t content_length, uint8_t *file_buffer,
    char *socket_buffer, char *log_entry, int cursor_pos, int total_buf_size, FILE *logfile) {
    int total_bytes_written = 0;
    flock(in_file, LOCK_EX);
    total_bytes_written = read_file_buffer(
        socket, in_file, content_length, file_buffer, socket_buffer, cursor_pos, total_buf_size);
    flock(in_file, LOCK_UN);

    pthread_mutex_lock(&loglock);
    put_201_response(socket);
    fwrite(log_entry, sizeof(log_entry[0]), strlen(log_entry), logfile);
    fflush(logfile);
    pthread_mutex_unlock(&loglock);
    return;
}

void put_request_response(int socket, int in_file, size_t content_length, uint8_t *file_buffer,
    char *socket_buffer, char *log_entry, int cursor_pos, int total_buf_size, FILE *logfile) {
    int total_bytes_written = 0;
    flock(in_file, LOCK_EX);
    total_bytes_written = read_file_buffer(
        socket, in_file, content_length, file_buffer, socket_buffer, cursor_pos, total_buf_size);
    flock(in_file, LOCK_UN);
    pthread_mutex_lock(&loglock);
    put_200_response(socket);
    fwrite(log_entry, sizeof(log_entry[0]), strlen(log_entry), logfile);
    fflush(logfile);
    pthread_mutex_unlock(&loglock);
    return;
}

void get_request_response(
    int out_file, int socket, uint8_t *file_buffer, char *uri, char *log_entry, FILE *logfile) {
    int total_written_bytes = 0;
    struct stat outfile;
    stat(uri + 1, &outfile);
    int of_size = outfile.st_size;
    if (of_size < 0) {
        internal_server_error(socket);
        return;
    }
    if (!of_size) {
        get_empty_response(socket);
        return;
    }
    char cl_string[25];
    memset(cl_string, 0, 25);
    char get_request_header[500];
    memset(get_request_header, 0, 500);
    strcat(get_request_header, stub_response_200);
    sprintf(cl_string, "%d", of_size);
    strcat(get_request_header, cl_string);
    strcat(get_request_header, "\r\n\r\n");

    write_bytes(socket, get_request_header, strlen(get_request_header));
    flock(out_file, LOCK_SH);
    total_written_bytes = write_file_buffer(socket, out_file, of_size, file_buffer);
    flock(out_file, LOCK_UN);
    pthread_mutex_lock(&loglock);
    fwrite(log_entry, sizeof(log_entry[0]), strlen(log_entry), logfile);
    fflush(logfile);
    pthread_mutex_unlock(&loglock);
    return;
}

void path_error_response(int path_check, int socket, char *log_entry, FILE *logfile) {
    if (path_check == 0) {
        pthread_mutex_lock(&loglock);
        not_found_error(socket);
        fwrite(log_entry, sizeof(log_entry[0]), strlen(log_entry), logfile);
        fflush(logfile);
        pthread_mutex_unlock(&loglock);
    }
    if (path_check == -1) {

        pthread_mutex_lock(&loglock);
        forbidden_path_error(socket);
        fwrite(log_entry, sizeof(log_entry[0]), strlen(log_entry), logfile);
        fflush(logfile);
        pthread_mutex_unlock(&loglock);
    }
    if (path_check == -2) {

        pthread_mutex_lock(&loglock);
        internal_server_error(socket);
        fwrite(log_entry, sizeof(log_entry[0]), strlen(log_entry), logfile);
        fflush(logfile);
        pthread_mutex_unlock(&loglock);
    }
}

void req_error_response(int req_check, int socket) {
    if (req_check == 0) {
        bad_request_error(socket);
    }
    if (req_check == -1) {
        internal_server_error(socket);
    }
}
