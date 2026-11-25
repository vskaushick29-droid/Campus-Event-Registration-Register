#ifndef MODELS_H
#define MODELS_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_PARTICIPANTS    500
#define NUM_EVENTS          5

#define MAX_CONFIRMATION_LEN 32
#define MAX_REG_ID_LEN      32
#define MAX_NAME_LEN        80
#define MAX_DEPT_LEN        80
#define MAX_TIMESTAMP_LEN   32
#define PARTICIPANTS_FILE   "data/participants.txt"
#define SUMMARY_FILE        "data/event_summary.txt"

// Event Identifiers (0 through 4)
typedef enum {
    EVENT_PAPER_PRESENTATION = 0,
    EVENT_CODING_COMPETITION = 1,
    EVENT_PROJECT_EXPO       = 2,
    EVENT_QUIZ_COMPETITION   = 3,
    EVENT_TECHNICAL_DEBATE   = 4,
    EVENT_INVALID            = -1
} EventType;

// Participant registration record
typedef struct {
    char confirmation_no[MAX_CONFIRMATION_LEN]; // e.g., "CNF2026-1001"
    char reg_id[MAX_REG_ID_LEN];               // College Registration ID, e.g., "1912260163"
    char name[MAX_NAME_LEN];                   // e.g., "Kaushick S"
    char department[MAX_DEPT_LEN];             // e.g., "Computer Science and Engineering"
    int event_id;                              // 0 to 4
    char registered_at[MAX_TIMESTAMP_LEN];     // e.g., "2026-11-15 14:30:00"
} Participant;

// Array of 500 participant records with current count
typedef struct {
    Participant records[MAX_PARTICIPANTS];
    int count;
} ParticipantList;

// Event-wise statistics structure
typedef struct {
    int total;
    int event_counts[NUM_EVENTS];
    double event_percentages[NUM_EVENTS];
} EventStats;

// Event names lookup
static const char *const EVENT_NAMES[NUM_EVENTS] = {
    "Paper Presentation",
    "Coding Competition",
    "Project Expo",
    "Quiz Competition",
    "Technical Debate"
};

// Event codes lookup
static const char *const EVENT_CODES[NUM_EVENTS] = {
    "PP01",
    "CC02",
    "PE03",
    "QC04",
    "TD05"
};

// Helper function to get event name safely
static inline const char *get_event_name(int event_id) {
    if (event_id >= 0 && event_id < NUM_EVENTS) {
        return EVENT_NAMES[event_id];
    }
    return "Unknown Event";
}

// Helper function to get event code safely
static inline const char *get_event_code(int event_id) {
    if (event_id >= 0 && event_id < NUM_EVENTS) {
        return EVENT_CODES[event_id];
    }
    return "NA";
}

#endif // MODELS_H

