#ifndef __UTIL_H__
#define __UTIL_H__

#include <inttypes.h>
#include <stdbool.h>

//	returns 0 if request is malformed
//	returns -1 if an error occured during request check
//	returns position of the end of the request if request is valid
int verify_request(char *buf, char *method, char *uri, char *version, int total_buf_size);

//	returns 0 if request is malformed
//	returns -1 if an error occured during request check
//	returns position of the end of the request if request is valid
int grab_header(char *buf, char *header, int total_buf_size, int cursor_pos);

//	returns 0 if there is no content legnth or content length is 0
//	returns -1 is content length is malformed
//	returns content length otherwise
ssize_t get_content_length(char *header);

//	finds request id and pushes into request id string buffer
//	buffer remains unchanged (default
//	returns content length otherwise
void get_request_id(char *header, char *request_id);

//	returns 0 on no file found
//	returns -1 on forbidden or directory
//	returns -2 if other error
int check_path(char *uri);

//	returns 0 if no method is found
//	returns method number otherwise
int check_method(char *method);

//	flushes remaining bytes in the request_buffer
//	sends counter with number of bytes that were flushed
int flush_header(char *request_buffer, uint8_t *file_buffer, int cursor_pos, int total_buf_size);

void log_request(char *log_entry, char *method, char *uri, char *code, char *request_id);

#endif
