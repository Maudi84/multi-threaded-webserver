// Some code used from 13S Compression PA
#include <stdio.h> //	sscanf()
#include <string.h> //	strstr() strcat() strcmp() memcpy()
#include <sys/stat.h> //	stat()
#include <regex.h> //	regexec() regcomp()
#include <errno.h> //	errno()
#include <ctype.h> //	isdigit()
// Begin user defined headers
#include "util.h"

const char *methods[] = { "GET", "PUT", "APPEND" };
const char *content_string = "Content-Length: ";
const char *request_id_string = "Request-Id: ";
const int method_types = 3;

// Begin functions
int check_method_format(char *method) {
    if (method[0] == '\0') {
        return 0;
    }
    if (strlen(method) > 8) {
        return 0;
    }
    regex_t regex;
    int reg_check;
    reg_check = regcomp(&regex, "^[a-zA-Z]+$", REG_EXTENDED);
    if (reg_check) {
        regfree(&regex);
        return -1;
    }
    reg_check = regexec(&regex, method, 0, NULL, 0);
    if (reg_check == REG_NOMATCH) {
        regfree(&regex);
        return 0;
    }
    if (reg_check) {
        regfree(&regex);
        return -1;
    }
    regfree(&regex);
    return 1;
}

int flush_header(char *socket_buffer, uint8_t *file_buffer, int cursor_pos, int total_buf_size) {
    if (cursor_pos + 2 > total_buf_size) {
        return 0;
    }
    int counter = 0;
    for (size_t i = 0; cursor_pos + 1 < total_buf_size; i++, cursor_pos++) {
        file_buffer[i] = socket_buffer[cursor_pos + 1];
        counter++;
    }
    return counter;
}

int check_uri_format(char *uri) {
    if (uri[0] == '\0') {
        return 0;
    }
    if (uri[0] != '/') {
        return 0;
    }
    if (strlen(uri) > 20) {
        return 0;
    }
    if (strlen(uri) < 2) {
        return 0;
    }
    regex_t uri_regex;
    int reg_check;
    reg_check = regcomp(&uri_regex, "^[a-zA-Z0-9_.\\/]+$", REG_EXTENDED);
    if (reg_check) {
        regfree(&uri_regex);
        return -1;
    }
    reg_check = regexec(&uri_regex, uri, 0, NULL, 0);
    if (reg_check == REG_NOMATCH) {
        regfree(&uri_regex);
        return 0;
    }
    if (reg_check) {
        regfree(&uri_regex);
        return -1;
    }
    regfree(&uri_regex);
    return 1;
}

int check_version_format(char *version) {
    if (version[0] == '\0') {
        return 0;
    }
    if (strlen(version) > 8) {
        return 0;
    }
    if (strlen(version) < 8) {
        return 0;
    }
    regex_t version_regex;
    int reg_check;
    reg_check = regcomp(&version_regex, "HTTP/1\\.1", REG_EXTENDED);
    if (reg_check) {
        regfree(&version_regex);
        return -1;
    }
    reg_check = regexec(&version_regex, version, 0, NULL, 0);
    if (reg_check == REG_NOMATCH) {
        regfree(&version_regex);
        return 0;
    }
    if (reg_check) {
        regfree(&version_regex);
        return -1;
    }
    regfree(&version_regex);
    return 1;
}

void parse_request(char *buf, char *method, char *uri, char *version) {

    sscanf(buf, "%9s %19s %9s", method, uri, version);
}

int verify_request(char *buf, char *method, char *uri, char *version, int total_buf_size) {
    parse_request(buf, method, uri, version);
    int method_check = check_method_format(method);
    if (!method_check) {
        return 0;
    }
    if (method_check < 0) {
        return -1;
    }
    int uri_check = check_uri_format(uri);
    if (!uri_check) {
        return 0;
    }
    if (uri_check < 0) {
        return -1;
    }
    int version_check = check_version_format(version);
    if (!version_check) {
        return 0;
    }
    if (version_check < 0) {
        return -1;
    }
    int new_cursor_pos = 0;
    new_cursor_pos = (strlen(method) + strlen(uri) + strlen(version) + 3);
    if (new_cursor_pos > total_buf_size) {
        return 0;
    }
    if (buf[new_cursor_pos - 1] != '\r') {
        return 0;
    }
    if (buf[new_cursor_pos] != '\n') {
        return 0;
    }
    return new_cursor_pos;
}

int check_method(char *method) {
    for (int i = 0; i < method_types; i++) {
        int cmp = strcmp(methods[i], method);
        if (cmp == 0) {
            return (i + 1);
        }
    }
    return 0;
}

int check_path(char *uri) {
    struct stat file_path;
    int file_check = stat(uri + 1, &file_path);
    if (file_check < 0) {
        if (errno == ENOENT) {
            return 0;
        } else {
            if (errno == EACCES) {
                return -1;
            } else {
                return -2;
            }
        }
    } else {
        if ((file_path.st_mode & S_IFMT) == S_IFDIR) {
            return -1;
        }
    }
    return 1;
}

void log_request(char *log_entry, char *method, char *uri, char *code, char *request_id) {
    strcat(log_entry, method);
    strcat(log_entry, ",");
    strcat(log_entry, uri);
    strcat(log_entry, ",");
    strcat(log_entry, code);
    strcat(log_entry, ",");
    strcat(log_entry, request_id);
    strcat(log_entry, "\n");
}

void get_request_id(char *header, char *request_id) {

    char *content = strstr(header, request_id_string);

    if (!content) {
        return;
    }
    size_t pos = content - header;
    if (pos) {
        if (!isspace(header[pos - 1])) {
            return;
        }
    }
    content = content + 12;
    for (; isspace(content[0]) && strlen(content) > 0; content++) {
    }
    if (!strlen(content) || !isdigit(content[0])) {
        return;
    }
    int i = 0;
    for (; isdigit(content[0]) && strlen(content) > 0; ++content, ++i) {
        request_id[i] = content[0];
    }
    if (content[0] != '\r' && content[1] != '\n') {
        return;
    }
    request_id[i + 1] = '\0';
    return;
}

ssize_t get_content_length(char *header) {
    int content_length = 0;
    char *content = strstr(header, content_string);
    if (!content) {
        return 0;
    }
    size_t pos = content - header;
    if (pos) {
        if (!isspace(header[pos - 1])) {
            return 0;
        }
    }
    content = content + 16;
    for (; isspace(content[0]) && strlen(content) > 0; content++) {
    }
    if (!strlen(content) || !isdigit(content[0])) {
        return 0;
    }
    content_length = content[0] - '0';
    ++content;
    for (; isdigit(content[0]) && strlen(content) > 0; ++content) {
        content_length = content_length * 10;
        content_length = content_length + (content[0] - '0');
    }
    if (content[0] != '\r' && content[1] != '\n') {
        return 0;
    }
    return content_length;
}

int grab_header(char *buf, char *header, int total_buf_size, int cursor_pos) {
    int req_size = cursor_pos;
    if (buf[cursor_pos + 1] == '\r' && buf[cursor_pos + 2] == '\n') {
        cursor_pos = cursor_pos + 2;
        return cursor_pos;
    }
    while (cursor_pos + 1 < total_buf_size) {
        for (int i = cursor_pos + 1; buf[i] != ':' || buf[i + 1] != ' '; i++, cursor_pos++) {
            if (cursor_pos + 2 > total_buf_size) {
                return 0;
            }
        }
        cursor_pos = cursor_pos + 2;
        if (buf[cursor_pos + 1] == '\r' && buf[cursor_pos + 2] == '\n') {
            return 0;
        }
        for (int i = cursor_pos + 1; buf[i] != '\r' || buf[i + 1] != '\n'; i++, cursor_pos++) {
            if (cursor_pos + 2 > total_buf_size) {
                return 0;
            }
        }
        cursor_pos = cursor_pos + 2;
        if (buf[cursor_pos + 1] == '\r' && buf[cursor_pos + 2] == '\n') {
            cursor_pos = cursor_pos + 2;
            int header_size = (cursor_pos + 1) - req_size;

            memcpy(header, buf + req_size + 1, header_size);
            header[header_size] = '\0';
            return cursor_pos;
        }
    }
    return 0;
}
