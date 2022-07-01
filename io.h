#ifndef __IO_H__
#define __IO_H__

#include <inttypes.h>
#include <stdbool.h>
#include <pthread.h> //	pthread_mutex_t
#include <stdio.h> //	FILE *

//	reads file size
//	if file size is zero send skeleton response
//	otherwise builds response and send data to socket
//	logs response
void get_request_response(
    int out_file, int socket, uint8_t *file_buffer, char *uri, char *log_entry, FILE *logfile);

//	sends not implemented error for unfound method
void not_implemented_error(int socket);

//	reads in bytes from the requested file
//	sends a 201 response once completed
void put_create_response(int socket, int in_file, size_t content_length, uint8_t *file_buffer,
    char *socket_buffer, char *log_entry, int cursor_pos, int total_buf_size, FILE *logfile);

//	reads in bytes from the requested file
//	sends a 201 response once completed
void put_request_response(int socket, int in_file, size_t content_length, uint8_t *file_buffer,
    char *socket_buffer, char *log_etnry, int cursor_pos, int total_buf_size, FILE *logfile);

//	sends not implemented response for method to socket
void not_implemented_error(int socket);

//	sends 404 if path_check is 0
//	sends 403 if path check is -1
//	sends 500 if path check is -2
void path_error_response(int path_check, int socket, char *log_entry, FILE *logfile);

//	sends 400 if req_check is 0
//	sends 500 ir req_check is -1
void req_error_response(int req_check, int socket);

#endif
