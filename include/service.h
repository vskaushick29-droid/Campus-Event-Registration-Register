#ifndef SERVICE_H
#define SERVICE_H

#include "models.h"

// Initialize service and load database
int service_init(ParticipantList *list);

// Linear search to check if a student with reg_id is already registered for event_id
// Returns index if already registered for this event, or -1 if not found
int service_find_event_registration(const ParticipantList *list, const char *reg_id, int event_id);

// Generates a unique, persistent confirmation number (e.g., CNF2026-1001)
void service_generate_confirmation_no(const ParticipantList *list, char *out_buf, size_t out_size);

// Registers a new participant:
// 1. Validates inputs
// 2. Checks capacity limit (500)
// 3. Performs linear search for duplicate ID
// 4. Appends to list and data/participants.txt
int service_register(ParticipantList *list,
                     const char *raw_reg_id,
                     const char *raw_name,
                     const char *raw_dept,
                     int event_id,
                     Participant *out_participant,
                     char *err_msg,
                     size_t err_size);

// Calculates event-wise counts and percentages
void service_get_stats(const ParticipantList *list, EventStats *stats);

// Linear search and filter:
// Matches search_term against reg_id, name, and department (case-insensitive)
// Filters by event_id (-1 for all events)
// Returns count of matching records stored in results
int service_search_and_filter(const ParticipantList *list,
                              const char *search_term,
                              int event_filter,
                              Participant *results,
                              int max_results);

// Admin authentication
int service_admin_login(const char *username, const char *password, char *token_out, size_t token_size);
int service_validate_token(const char *token);

#endif // SERVICE_H

