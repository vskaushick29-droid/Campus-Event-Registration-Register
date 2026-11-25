#ifndef HTTP_SERVER_H
#define HTTP_SERVER_H

#include <winsock2.h>
#include "models.h"

// Starts the HTTP server on specified port
int http_server_start(int port, ParticipantList *list);

#endif // HTTP_SERVER_H

