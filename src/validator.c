#include <ctype.h>
#include <string.h>
#include <stdio.h>
#include "validator.h"

// Trims leading and trailing whitespace
static void trim_whitespace(char *str) {
    if (!str) return;
    char *start = str;
    while (*start && isspace((unsigned char)*start)) {
        start++;
    }
    char *end = start + strlen(start);
    while (end > start && isspace((unsigned char)*(end - 1))) {
        end--;
    }
    *end = '\0';
    if (start != str) {
        memmove(str, start, end - start + 1);
    }
}

void sanitize_text(char *dest, const char *src, size_t max_len) {
    if (!dest || !src || max_len == 0) return;
    size_t d = 0;
    for (size_t s = 0; src[s] != '\0' && d < max_len - 1; s++) {
        char ch = src[s];
        // Replace delimiter and newline characters with a space
        if (ch == '|' || ch == '\r' || ch == '\n') {
            ch = ' ';
        }
        dest[d++] = ch;
    }
    dest[d] = '\0';
    trim_whitespace(dest);
}

int validate_reg_id(const char *reg_id, char *err_msg, size_t err_size) {
    if (!reg_id || strlen(reg_id) == 0) {
        snprintf(err_msg, err_size, "Registration ID cannot be empty.");
        return 0;
    }
    size_t len = strlen(reg_id);
    if (len < 3 || len >= MAX_REG_ID_LEN) {
        snprintf(err_msg, err_size, "Registration ID must be between 3 and %d characters.", MAX_REG_ID_LEN - 1);
        return 0;
    }
    for (size_t i = 0; i < len; i++) {
        char c = reg_id[i];
        if (!isalnum((unsigned char)c) && c != '-' && c != '_') {
            snprintf(err_msg, err_size, "Registration ID contains invalid characters. Only letters, numbers, '-' and '_' are allowed.");
            return 0;
        }
    }
    return 1;
}

int validate_name(const char *name, char *err_msg, size_t err_size) {
    if (!name || strlen(name) == 0) {
        snprintf(err_msg, err_size, "Student Name cannot be empty.");
        return 0;
    }
    size_t len = strlen(name);
    if (len < 2 || len >= MAX_NAME_LEN) {
        snprintf(err_msg, err_size, "Student Name must be between 2 and %d characters.", MAX_NAME_LEN - 1);
        return 0;
    }
    for (size_t i = 0; i < len; i++) {
        char c = name[i];
        if (!isalpha((unsigned char)c) && !isspace((unsigned char)c) && c != '.' && c != '\'') {
            snprintf(err_msg, err_size, "Student Name contains invalid characters. Only letters, spaces, and dots are allowed.");
            return 0;
        }
    }
    return 1;
}

int validate_department(const char *dept, char *err_msg, size_t err_size) {
    if (!dept || strlen(dept) == 0) {
        snprintf(err_msg, err_size, "Department cannot be empty.");
        return 0;
    }
    size_t len = strlen(dept);
    if (len < 2 || len >= MAX_DEPT_LEN) {
        snprintf(err_msg, err_size, "Department must be between 2 and %d characters.", MAX_DEPT_LEN - 1);
        return 0;
    }
    return 1;
}

int validate_event_id(int event_id, char *err_msg, size_t err_size) {
    if (event_id < 0 || event_id >= NUM_EVENTS) {
        snprintf(err_msg, err_size, "Invalid Event selection. Must be between 0 and %d.", NUM_EVENTS - 1);
        return 0;
    }
    return 1;
}

