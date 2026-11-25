#ifndef JSON_UTILS_H
#define JSON_UTILS_H

#include <winsock2.h>
#include <stddef.h>

// Extracts a string value from a simple JSON object
int json_extract_string(const char *json, const char *key, char *out_val, size_t out_size);

// Extracts an integer value from a simple JSON object
int json_extract_int(const char *json, const char *key, int *out_val);

// Escapes special characters for JSON output
void json_escape(const char *src, char *dest, size_t dest_size);

// Sends a complete HTTP JSON response with CORS headers
void json_send_response(SOCKET sock, int status_code, const char *status_text, const char *json_body);

// Convenience response helpers
void json_send_error(SOCKET sock, int status_code, const char *error_msg);
void json_send_success(SOCKET sock, int status_code, const char *success_msg);

#endif // JSON_UTILS_H

