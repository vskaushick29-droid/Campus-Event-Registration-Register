#ifndef VALIDATOR_H
#define VALIDATOR_H

#include <stddef.h>
#include "models.h"

// Validation functions
int validate_reg_id(const char *reg_id, char *err_msg, size_t err_size);
int validate_name(const char *name, char *err_msg, size_t err_size);
int validate_department(const char *dept, char *err_msg, size_t err_size);
int validate_event_id(int event_id, char *err_msg, size_t err_size);

// Sanitizes user input by stripping delimiters ('|', '\r', '\n')
void sanitize_text(char *dest, const char *src, size_t max_len);

#endif // VALIDATOR_H

