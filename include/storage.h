#ifndef STORAGE_H
#define STORAGE_H

#include "models.h"

// Loads all participants from file into memory
int storage_load_participants(const char *filename, ParticipantList *list);

// Appends a single participant to the file
int storage_append_participant(const char *filename, const Participant *p);

// Generates the event_summary.txt file
int storage_generate_summary(const char *summary_filename, const ParticipantList *list);

#endif // STORAGE_H

