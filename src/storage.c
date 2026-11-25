#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "storage.h"

int storage_load_participants(const char *filename, ParticipantList *list) {
    if (!list) return 0;
    list->count = 0;

    FILE *file = fopen(filename, "r");
    if (!file) {
        // If file doesn't exist, try creating an empty one
        file = fopen(filename, "w");
        if (file) {
            fclose(file);
        }
        return 0;
    }

    char line[512];
    while (fgets(line, sizeof(line), file)) {
        if (list->count >= MAX_PARTICIPANTS) {
            break;
        }

        // Strip trailing newline characters
        size_t len = strlen(line);
        while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r')) {
            line[--len] = '\0';
        }

        // Skip comments or empty lines
        if (len == 0 || line[0] == '#') {
            continue;
        }

        Participant p;
        memset(&p, 0, sizeof(Participant));

        int pipe_count = 0;
        for (size_t i = 0; i < len; i++) {
            if (line[i] == '|') pipe_count++;
        }

        if (pipe_count >= 5) {
            // Standard format: CONFIRMATION_NO|REG_ID|NAME|DEPARTMENT|EVENT_ID|REGISTERED_AT
            char *token = line;
            char *pipe1 = strchr(token, '|');
            if (!pipe1) continue;
            *pipe1 = '\0';
            strncpy(p.confirmation_no, token, MAX_CONFIRMATION_LEN - 1);

            token = pipe1 + 1;
            char *pipe2 = strchr(token, '|');
            if (!pipe2) continue;
            *pipe2 = '\0';
            strncpy(p.reg_id, token, MAX_REG_ID_LEN - 1);

            token = pipe2 + 1;
            char *pipe3 = strchr(token, '|');
            if (!pipe3) continue;
            *pipe3 = '\0';
            strncpy(p.name, token, MAX_NAME_LEN - 1);

            token = pipe3 + 1;
            char *pipe4 = strchr(token, '|');
            if (!pipe4) continue;
            *pipe4 = '\0';
            strncpy(p.department, token, MAX_DEPT_LEN - 1);

            token = pipe4 + 1;
            char *pipe5 = strchr(token, '|');
            if (!pipe5) continue;
            *pipe5 = '\0';
            p.event_id = atoi(token);

            token = pipe5 + 1;
            strncpy(p.registered_at, token, MAX_TIMESTAMP_LEN - 1);
        } else if (pipe_count == 4) {
            // Legacy fallback: REG_ID|NAME|DEPARTMENT|EVENT_ID|REGISTERED_AT
            snprintf(p.confirmation_no, sizeof(p.confirmation_no), "CNF2026-%04d", 1001 + list->count);

            char *token = line;
            char *pipe1 = strchr(token, '|');
            if (!pipe1) continue;
            *pipe1 = '\0';
            strncpy(p.reg_id, token, MAX_REG_ID_LEN - 1);

            token = pipe1 + 1;
            char *pipe2 = strchr(token, '|');
            if (!pipe2) continue;
            *pipe2 = '\0';
            strncpy(p.name, token, MAX_NAME_LEN - 1);

            token = pipe2 + 1;
            char *pipe3 = strchr(token, '|');
            if (!pipe3) continue;
            *pipe3 = '\0';
            strncpy(p.department, token, MAX_DEPT_LEN - 1);

            token = pipe3 + 1;
            char *pipe4 = strchr(token, '|');
            if (!pipe4) continue;
            *pipe4 = '\0';
            p.event_id = atoi(token);

            token = pipe4 + 1;
            strncpy(p.registered_at, token, MAX_TIMESTAMP_LEN - 1);
        } else {
            continue; // Skip malformed lines
        }

        // Store into list
        list->records[list->count] = p;
        list->count++;
    }

    fclose(file);
    return list->count;
}

int storage_append_participant(const char *filename, const Participant *p) {
    if (!p) return 0;

    FILE *file = fopen(filename, "a");
    if (!file) {
        return 0;
    }

    fprintf(file, "%s|%s|%s|%s|%d|%s\n",
            p->confirmation_no,
            p->reg_id,
            p->name,
            p->department,
            p->event_id,
            p->registered_at);

    fflush(file);
    fclose(file);
    return 1;
}

int storage_generate_summary(const char *summary_filename, const ParticipantList *list) {
    if (!list) return 0;

    FILE *file = fopen(summary_filename, "w");
    if (!file) {
        return 0;
    }

    // Calculate statistics
    int event_counts[NUM_EVENTS] = {0};
    int total = list->count;

    for (int i = 0; i < total; i++) {
        int ev = list->records[i].event_id;
        if (ev >= 0 && ev < NUM_EVENTS) {
            event_counts[ev]++;
        }
    }

    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    char time_str[64];
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", t);

    // Formatted ASCII Report
    fprintf(file, "================================================================================\n");
    fprintf(file, "                    CAMPUS EVENT REGISTRATION SUMMARY REPORT                    \n");
    fprintf(file, "                     Generated on: %s                    \n", time_str);
    fprintf(file, "================================================================================\n\n");
    fprintf(file, "Total Registered Participants: %d / %d\n", total, MAX_PARTICIPANTS);
    fprintf(file, "Available Remaining Capacity : %d\n\n", MAX_PARTICIPANTS - total);

    fprintf(file, "+--------------------------------+--------+-------------------+----------------+\n");
    fprintf(file, "| Event Name                     | Code   | Registered Count  | Share (%%)      |\n");
    fprintf(file, "+--------------------------------+--------+-------------------+----------------+\n");

    for (int j = 0; j < NUM_EVENTS; j++) {
        double pct = (total > 0) ? ((double)event_counts[j] / total) * 100.0 : 0.0;
        fprintf(file, "| %-30s | %-6s | %-17d | %13.2f%% |\n",
                get_event_name(j),
                get_event_code(j),
                event_counts[j],
                pct);
    }
    fprintf(file, "+--------------------------------+--------+-------------------+----------------+\n\n");

    fprintf(file, "================================================================================\n");
    fprintf(file, "                            RECENT REGISTRATION AUDIT                           \n");
    fprintf(file, "================================================================================\n");
    int display_recent = total > 10 ? 10 : total;
    if (total == 0) {
        fprintf(file, "No participants registered yet.\n");
    } else {
        fprintf(file, "Showing last %d registration(s):\n", display_recent);
        fprintf(file, "%-16s %-14s %-22s %-22s %-20s\n", "CONFIRMATION NO", "REG ID", "NAME", "DEPARTMENT", "EVENT");
        fprintf(file, "----------------------------------------------------------------------------------------------------\n");
        for (int i = total - display_recent; i < total; i++) {
            const Participant *p = &list->records[i];
            fprintf(file, "%-16s %-14s %-22.22s %-22.22s %-20.20s\n",
                    p->confirmation_no,
                    p->reg_id,
                    p->name,
                    p->department,
                    get_event_name(p->event_id));
        }
    }
    fprintf(file, "\n================================================================================\n");
    fprintf(file, "Report End.\n");

    fflush(file);
    fclose(file);
    return 1;
}
