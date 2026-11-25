#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include "service.h"
#include "storage.h"
#include "validator.h"

// In-memory admin session storage
static char g_active_token[64] = "";
static time_t g_token_expiry = 0;

// Helper: Case-insensitive string equality
static int str_case_equals(const char *s1, const char *s2) {
    if (!s1 || !s2) return 0;
    while (*s1 && *s2) {
        if (tolower((unsigned char)*s1) != tolower((unsigned char)*s2)) {
            return 0;
        }
        s1++;
        s2++;
    }
    return (*s1 == '\0' && *s2 == '\0');
}

// Helper: Case-insensitive substring search (linear scan)
static int str_case_contains(const char *haystack, const char *needle) {
    if (!haystack || !needle) return 0;
    if (*needle == '\0') return 1; // Empty query matches all

    size_t needle_len = strlen(needle);
    size_t haystack_len = strlen(haystack);

    if (needle_len > haystack_len) return 0;

    for (size_t i = 0; i <= haystack_len - needle_len; i++) {
        size_t j = 0;
        while (j < needle_len &&
               tolower((unsigned char)haystack[i + j]) == tolower((unsigned char)needle[j])) {
            j++;
        }
        if (j == needle_len) {
            return 1;
        }
    }
    return 0;
}

int service_init(ParticipantList *list) {
    if (!list) return 0;
    return storage_load_participants(PARTICIPANTS_FILE, list);
}

// Linear search to check if a student with reg_id is already registered for event_id
int service_find_event_registration(const ParticipantList *list, const char *reg_id, int event_id) {
    if (!list || !reg_id) return -1;

    for (int i = 0; i < list->count; i++) {
        const Participant *p = &list->records[i];
        if (str_case_equals(p->reg_id, reg_id) && p->event_id == event_id) {
            return i; // Already registered for this specific event
        }
    }
    return -1; // Not registered for this event
}

// Generates a unique, persistent confirmation number (e.g., CNF2026-1001)
void service_generate_confirmation_no(const ParticipantList *list, char *out_buf, size_t out_size) {
    if (!out_buf || out_size == 0) return;

    int max_seq = 1000;
    if (list) {
        for (int i = 0; i < list->count; i++) {
            const char *c = list->records[i].confirmation_no;
            const char *dash = strrchr(c, '-');
            if (dash) {
                int seq = atoi(dash + 1);
                if (seq > max_seq) {
                    max_seq = seq;
                }
            } else {
                int seq = 1000 + i + 1;
                if (seq > max_seq) {
                    max_seq = seq;
                }
            }
        }
    }

    int next_seq = max_seq + 1;
    snprintf(out_buf, out_size, "CNF2026-%04d", next_seq);
}

int service_register(ParticipantList *list,
                     const char *raw_reg_id,
                     const char *raw_name,
                     const char *raw_dept,
                     int event_id,
                     Participant *out_participant,
                     char *err_msg,
                     size_t err_size) {
    if (!list) {
        snprintf(err_msg, err_size, "Internal error: Participant database is uninitialized.");
        return 0;
    }

    // 1. Capacity Check
    if (list->count >= MAX_PARTICIPANTS) {
        snprintf(err_msg, err_size, "Registration limit reached! Maximum capacity of %d participants exceeded.", MAX_PARTICIPANTS);
        return 0;
    }

    // 2. Sanitize inputs
    char clean_reg_id[MAX_REG_ID_LEN];
    char clean_name[MAX_NAME_LEN];
    char clean_dept[MAX_DEPT_LEN];

    sanitize_text(clean_reg_id, raw_reg_id, sizeof(clean_reg_id));
    sanitize_text(clean_name, raw_name, sizeof(clean_name));
    sanitize_text(clean_dept, raw_dept, sizeof(clean_dept));

    // 3. Validation checks
    if (!validate_reg_id(clean_reg_id, err_msg, err_size)) {
        return 0;
    }
    if (!validate_name(clean_name, err_msg, err_size)) {
        return 0;
    }
    if (!validate_department(clean_dept, err_msg, err_size)) {
        return 0;
    }
    if (!validate_event_id(event_id, err_msg, err_size)) {
        return 0;
    }

    // 4. Duplicate Check via Linear Search:
    // Disallow registering for the SAME event more than once with the same Registration ID
    int dup_idx = service_find_event_registration(list, clean_reg_id, event_id);
    if (dup_idx != -1) {
        snprintf(err_msg, err_size,
                 "Registration ID '%s' is already registered for event '%s'. Each student can register for each event only once.",
                 clean_reg_id,
                 get_event_name(event_id));
        return 0;
    }

    // 5. Construct new record
    Participant new_p;
    memset(&new_p, 0, sizeof(Participant));

    // Generate unique Confirmation Number
    service_generate_confirmation_no(list, new_p.confirmation_no, sizeof(new_p.confirmation_no));

    strncpy(new_p.reg_id, clean_reg_id, MAX_REG_ID_LEN - 1);
    strncpy(new_p.name, clean_name, MAX_NAME_LEN - 1);
    strncpy(new_p.department, clean_dept, MAX_DEPT_LEN - 1);
    new_p.event_id = event_id;

    // Timestamp
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    strftime(new_p.registered_at, sizeof(new_p.registered_at), "%Y-%m-%d %H:%M:%S", t);

    // 6. Append to in-memory array using pointer
    list->records[list->count] = new_p;
    list->count++;

    // 7. Persist to data/participants.txt
    if (!storage_append_participant(PARTICIPANTS_FILE, &new_p)) {
        // Rollback memory state if file write fails
        list->count--;
        snprintf(err_msg, err_size, "Failed to write participant record to storage file.");
        return 0;
    }

    if (out_participant) {
        *out_participant = new_p;
    }

    return 1;
}

void service_get_stats(const ParticipantList *list, EventStats *stats) {
    if (!stats) return;
    memset(stats, 0, sizeof(EventStats));

    if (!list) return;

    stats->total = list->count;

    // Linear count for each event
    for (int i = 0; i < list->count; i++) {
        int ev = list->records[i].event_id;
        if (ev >= 0 && ev < NUM_EVENTS) {
            stats->event_counts[ev]++;
        }
    }

    // Calculate percentages
    for (int j = 0; j < NUM_EVENTS; j++) {
        if (stats->total > 0) {
            stats->event_percentages[j] = ((double)stats->event_counts[j] / stats->total) * 100.0;
        } else {
            stats->event_percentages[j] = 0.0;
        }
    }
}

// Linear search and filtering over participants array
int service_search_and_filter(const ParticipantList *list,
                              const char *search_term,
                              int event_filter,
                              Participant *results,
                              int max_results) {
    if (!list || !results || max_results <= 0) return 0;

    int matched_count = 0;

    for (int i = 0; i < list->count; i++) {
        const Participant *p = &list->records[i];

        // 1. Filter by event
        if (event_filter >= 0 && event_filter < NUM_EVENTS) {
            if (p->event_id != event_filter) {
                continue;
            }
        }

        // 2. Filter by search query (linear substring search on Confirmation No, ID, Name, Department)
        if (search_term && strlen(search_term) > 0) {
            int matches = str_case_contains(p->confirmation_no, search_term) ||
                          str_case_contains(p->reg_id, search_term) ||
                          str_case_contains(p->name, search_term) ||
                          str_case_contains(p->department, search_term);
            if (!matches) {
                continue;
            }
        }

        // Add to result buffer
        if (matched_count < max_results) {
            results[matched_count] = *p;
            matched_count++;
        }
    }

    return matched_count;
}

int service_admin_login(const char *username, const char *password, char *token_out, size_t token_size) {
    if (!username || !password || !token_out || token_size == 0) {
        return 0;
    }

    // Default admin credentials for campus event registration register
    if (strcmp(username, "admin") == 0 && strcmp(password, "admin123") == 0) {
        srand((unsigned int)time(NULL));
        snprintf(g_active_token, sizeof(g_active_token), "adm_sec_%08x%08x%04x",
                 (unsigned int)rand(), (unsigned int)rand(), (unsigned int)(time(NULL) & 0xFFFF));

        // Token valid for 24 hours
        g_token_expiry = time(NULL) + (24 * 3600);

        strncpy(token_out, g_active_token, token_size - 1);
        token_out[token_size - 1] = '\0';
        return 1;
    }

    return 0;
}

int service_validate_token(const char *token) {
    if (!token || strlen(token) == 0) return 0;
    if (strlen(g_active_token) == 0) return 0;

    // Check expiry
    if (time(NULL) > g_token_expiry) {
        g_active_token[0] = '\0';
        return 0;
    }

    return (strcmp(token, g_active_token) == 0);
}
