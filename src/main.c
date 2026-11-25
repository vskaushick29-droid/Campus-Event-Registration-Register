#include <stdio.h>
#include <stdlib.h>
#include "models.h"
#include "service.h"
#include "http_server.h"

int main(int argc, char *argv[]) {
    int port = 8080;
    if (argc > 1) {
        int custom_port = atoi(argv[1]);
        if (custom_port > 1024 && custom_port < 65535) {
            port = custom_port;
        }
    }

    ParticipantList list;
    memset(&list, 0, sizeof(ParticipantList));

    // Initialize database from file
    service_init(&list);

    // Start HTTP server (blocks in listening loop)
    http_server_start(port, &list);

    return 0;
}
