#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "json_utils.h"

int json_extract_string(const char *json, const char *key, char *out_val, size_t out_size) {
    if (!json || !key || !out_val || out_size == 0) return 0;
    out_val[0] = '\0';

    char pattern[128];
    snprintf(pattern, sizeof(pattern), "\"%s\"", key);

    const char *pos = strstr(json, pattern);
    if (!pos) return 0;

    pos += strlen(pattern);
    // Skip whitespace and colon
    while (*pos && (*pos == ' ' || *pos == '\t' || *pos == ':' || *pos == '\r' || *pos == '\n')) {
        pos++;
    }

    if (*pos != '\"') return 0;
    pos++; // Skip opening quote

    size_t i = 0;
    while (*pos && *pos != '\"' && i < out_size - 1) {
        if (*pos == '\\' && *(pos + 1) != '\0') {
            pos++; // handle simple escaped characters
        }
        out_val[i++] = *pos++;
    }
    out_val[i] = '\0';
    return 1;
}

int json_extract_int(const char *json, const char *key, int *out_val) {
    if (!json || !key || !out_val) return 0;

    char pattern[128];
    snprintf(pattern, sizeof(pattern), "\"%s\"", key);

    const char *pos = strstr(json, pattern);
    if (!pos) return 0;

    pos += strlen(pattern);
    while (*pos && (*pos == ' ' || *pos == '\t' || *pos == ':' || *pos == '\r' || *pos == '\n')) {
        pos++;
    }

    // Support both quoted integer "1" and bare integer 1
    if (*pos == '\"') {
        pos++;
    }

    if (!isdigit((unsigned char)*pos) && *pos != '-') {
        return 0;
    }

    *out_val = atoi(pos);
    return 1;
}

void json_escape(const char *src, char *dest, size_t dest_size) {
    if (!src || !dest || dest_size == 0) return;
    size_t d = 0;
    for (size_t s = 0; src[s] != '\0' && d < dest_size - 1; s++) {
        char c = src[s];
        if (c == '\"' || c == '\\') {
            if (d < dest_size - 2) {
                dest[d++] = '\\';
                dest[d++] = c;
            }
        } else if (c == '\n') {
            if (d < dest_size - 2) {
                dest[d++] = '\\';
                dest[d++] = 'n';
            }
        } else if (c == '\r') {
            // skip carriage return
        } else if (c == '\t') {
            if (d < dest_size - 2) {
                dest[d++] = '\\';
                dest[d++] = 't';
            }
        } else {
            dest[d++] = c;
        }
    }
    dest[d] = '\0';
}

void json_send_response(SOCKET sock, int status_code, const char *status_text, const char *json_body) {
    char header_buf[1024];
    int body_len = json_body ? (int)strlen(json_body) : 0;

    int header_len = snprintf(header_buf, sizeof(header_buf),
        "HTTP/1.1 %d %s\r\n"
        "Content-Type: application/json; charset=UTF-8\r\n"
        "Content-Length: %d\r\n"
        "Access-Control-Allow-Origin: *\r\n"
        "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n"
        "Access-Control-Allow-Headers: Content-Type, Authorization\r\n"
        "Connection: close\r\n\r\n",
        status_code, status_text, body_len);

    send(sock, header_buf, header_len, 0);
    if (body_len > 0) {
        send(sock, json_body, body_len, 0);
    }
}

void json_send_error(SOCKET sock, int status_code, const char *error_msg) {
    char clean_msg[256];
    json_escape(error_msg, clean_msg, sizeof(clean_msg));

    char body[512];
    snprintf(body, sizeof(body), "{\"status\":\"error\",\"message\":\"%s\"}", clean_msg);

    const char *status_text = "Bad Request";
    if (status_code == 401) status_text = "Unauthorized";
    else if (status_code == 404) status_text = "Not Found";
    else if (status_code == 409) status_text = "Conflict";
    else if (status_code == 500) status_text = "Internal Server Error";

    json_send_response(sock, status_code, status_text, body);
}

void json_send_success(SOCKET sock, int status_code, const char *success_msg) {
    char clean_msg[256];
    json_escape(success_msg, clean_msg, sizeof(clean_msg));

    char body[512];
    snprintf(body, sizeof(body), "{\"status\":\"success\",\"message\":\"%s\"}", clean_msg);

    json_send_response(sock, status_code, "OK", body);
}

