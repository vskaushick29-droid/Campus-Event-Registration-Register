#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include "http_server.h"
#include "service.h"
#include "storage.h"
#include "json_utils.h"
#include "models.h"

#define REQ_BUF_SIZE 16384

static ParticipantList *g_list_ref = NULL;

// Helper: URL-decode in place or to destination
static void url_decode(char *dest, const char *src, size_t dest_len) {
    if (!dest || !src || dest_len == 0) return;
    size_t d = 0;
    for (size_t s = 0; src[s] != '\0' && d < dest_len - 1; s++) {
        if (src[s] == '%' && isxdigit((unsigned char)src[s + 1]) && isxdigit((unsigned char)src[s + 2])) {
            char hex[3] = { src[s + 1], src[s + 2], '\0' };
            dest[d++] = (char)strtol(hex, NULL, 16);
            s += 2;
        } else if (src[s] == '+') {
            dest[d++] = ' ';
        } else {
            dest[d++] = src[s];
        }
    }
    dest[d] = '\0';
}

// Helper: Extract query parameter value
static int get_query_param(const char *query, const char *key, char *out_val, size_t out_len) {
    if (!query || !key || !out_val || out_len == 0) return 0;
    out_val[0] = '\0';

    char pattern[64];
    snprintf(pattern, sizeof(pattern), "%s=", key);
    const char *pos = strstr(query, pattern);
    if (!pos) return 0;

    pos += strlen(pattern);
    char raw[256];
    size_t i = 0;
    while (*pos && *pos != '&' && i < sizeof(raw) - 1) {
        raw[i++] = *pos++;
    }
    raw[i] = '\0';

    url_decode(out_val, raw, out_len);
    return 1;
}

// Helper: Extract header value
static int get_header_value(const char *req, const char *header_name, char *out_val, size_t out_len) {
    if (!req || !header_name || !out_val || out_len == 0) return 0;
    out_val[0] = '\0';

    char pattern[128];
    snprintf(pattern, sizeof(pattern), "\n%s:", header_name);
    const char *pos = strstr(req, pattern);
    if (!pos) {
        // Try without leading newline for first line
        snprintf(pattern, sizeof(pattern), "%s:", header_name);
        pos = strstr(req, pattern);
        if (!pos) return 0;
    }

    pos += strlen(pattern);
    while (*pos == ' ' || *pos == '\t') pos++;

    size_t i = 0;
    while (*pos && *pos != '\r' && *pos != '\n' && i < out_len - 1) {
        out_val[i++] = *pos++;
    }
    out_val[i] = '\0';
    return 1;
}

// Helper: Extract admin auth token from Authorization header or Query
static int extract_admin_token(const char *req, const char *query, char *token_out, size_t token_size) {
    token_out[0] = '\0';

    // 1. Try Authorization: Bearer <token>
    char auth_header[128];
    if (get_header_value(req, "Authorization", auth_header, sizeof(auth_header))) {
        if (strncmp(auth_header, "Bearer ", 7) == 0) {
            strncpy(token_out, auth_header + 7, token_size - 1);
            token_out[token_size - 1] = '\0';
            return 1;
        }
    }

    // 2. Try X-Auth-Token header
    if (get_header_value(req, "X-Auth-Token", token_out, token_size)) {
        return 1;
    }

    // 3. Try query parameter ?token=...
    if (query && get_query_param(query, "token", token_out, token_size)) {
        return 1;
    }

    return 0;
}

// Serve static files from web/ directory
static void serve_static_file(SOCKET sock, const char *path) {
    char filepath[256];
    if (strcmp(path, "/") == 0 || strcmp(path, "/index.html") == 0) {
        snprintf(filepath, sizeof(filepath), "web/index.html");
    } else if (strcmp(path, "/admin") == 0 || strcmp(path, "/admin/") == 0 || strcmp(path, "/admin.html") == 0) {
        snprintf(filepath, sizeof(filepath), "web/admin.html");
    } else if (strncmp(path, "/css/", 5) == 0 || strncmp(path, "/js/", 4) == 0) {
        snprintf(filepath, sizeof(filepath), "web%s", path);
    } else {
        json_send_error(sock, 404, "Page Not Found");
        return;
    }

    FILE *f = fopen(filepath, "rb");
    if (!f) {
        json_send_error(sock, 404, "Static file not found");
        return;
    }

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    char *buf = (char *)malloc(size);
    if (!buf) {
        fclose(f);
        json_send_error(sock, 500, "Memory allocation error");
        return;
    }

    fread(buf, 1, size, f);
    fclose(f);

    const char *mime = "text/html";
    if (strstr(filepath, ".css")) mime = "text/css";
    else if (strstr(filepath, ".js")) mime = "application/javascript";

    char header[512];
    int hlen = snprintf(header, sizeof(header),
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: %s; charset=UTF-8\r\n"
        "Content-Length: %ld\r\n"
        "Connection: close\r\n\r\n", mime, size);

    send(sock, header, hlen, 0);
    send(sock, buf, size, 0);
    free(buf);
}

// Handler: OPTIONS for CORS preflight
static void handle_options(SOCKET sock) {
    const char *resp =
        "HTTP/1.1 204 No Content\r\n"
        "Access-Control-Allow-Origin: *\r\n"
        "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n"
        "Access-Control-Allow-Headers: Content-Type, Authorization, X-Auth-Token\r\n"
        "Access-Control-Max-Age: 86400\r\n"
        "Content-Length: 0\r\n"
        "Connection: close\r\n\r\n";
    send(sock, resp, (int)strlen(resp), 0);
}

// Handler: GET /api/events
static void handle_get_events(SOCKET sock) {
    char body[1024];
    int offset = snprintf(body, sizeof(body), "{\"status\":\"success\",\"events\":[");
    for (int i = 0; i < NUM_EVENTS; i++) {
        offset += snprintf(body + offset, sizeof(body) - offset,
            "%s{\"id\":%d,\"name\":\"%s\",\"code\":\"%s\"}",
            (i > 0 ? "," : ""),
            i,
            get_event_name(i),
            get_event_code(i));
    }
    snprintf(body + offset, sizeof(body) - offset, "]}");
    json_send_response(sock, 200, "OK", body);
}

// Handler: POST /api/register
static void handle_post_register(SOCKET sock, const char *body) {
    char reg_id[MAX_REG_ID_LEN] = "";
    char name[MAX_NAME_LEN] = "";
    char dept[MAX_DEPT_LEN] = "";
    int event_id = -1;

    json_extract_string(body, "reg_id", reg_id, sizeof(reg_id));
    json_extract_string(body, "name", name, sizeof(name));
    json_extract_string(body, "department", dept, sizeof(dept));
    if (!json_extract_int(body, "event_id", &event_id)) {
        // Try fallback key "event"
        json_extract_int(body, "event", &event_id);
    }

    char err_msg[256] = "";
    Participant saved_p;
    memset(&saved_p, 0, sizeof(saved_p));

    // Register participant
    int ok = service_register(g_list_ref, reg_id, name, dept, event_id, &saved_p, err_msg, sizeof(err_msg));
    if (!ok) {
        // Check if error was due to duplicate registration
        if (strstr(err_msg, "already registered")) {
            json_send_error(sock, 409, err_msg);
        } else {
            json_send_error(sock, 400, err_msg);
        }
        return;
    }

    // Construct response with registered participant details
    char clean_name[MAX_NAME_LEN * 2];
    char clean_dept[MAX_DEPT_LEN * 2];
    json_escape(saved_p.name, clean_name, sizeof(clean_name));
    json_escape(saved_p.department, clean_dept, sizeof(clean_dept));

    char resp_body[1024];
    snprintf(resp_body, sizeof(resp_body),
        "{"
        "\"status\":\"success\","
        "\"message\":\"Registration confirmed successfully!\","
        "\"participant\":{"
            "\"confirmation_no\":\"%s\","
            "\"reg_id\":\"%s\","
            "\"name\":\"%s\","
            "\"department\":\"%s\","
            "\"event_id\":%d,"
            "\"event_name\":\"%s\","
            "\"event_code\":\"%s\","
            "\"registered_at\":\"%s\""
        "}"
        "}",
        saved_p.confirmation_no,
        saved_p.reg_id,
        clean_name,
        clean_dept,
        saved_p.event_id,
        get_event_name(saved_p.event_id),
        get_event_code(saved_p.event_id),
        saved_p.registered_at);

    json_send_response(sock, 201, "Created", resp_body);
}

// Handler: POST /api/admin/login
static void handle_post_admin_login(SOCKET sock, const char *body) {
    char username[64] = "";
    char password[64] = "";

    json_extract_string(body, "username", username, sizeof(username));
    json_extract_string(body, "password", password, sizeof(password));

    char token[64];
    if (service_admin_login(username, password, token, sizeof(token))) {
        char resp_body[256];
        snprintf(resp_body, sizeof(resp_body),
            "{\"status\":\"success\",\"message\":\"Login successful\",\"token\":\"%s\",\"expires_in\":86400}",
            token);
        json_send_response(sock, 200, "OK", resp_body);
    } else {
        json_send_error(sock, 401, "Invalid admin username or password.");
    }
}

// Handler: GET /api/admin/stats
static void handle_get_admin_stats(SOCKET sock, const char *req, const char *query) {
    char token[64];
    if (!extract_admin_token(req, query, token, sizeof(token)) || !service_validate_token(token)) {
        json_send_error(sock, 401, "Unauthorized: Valid admin token required.");
        return;
    }

    EventStats stats;
    service_get_stats(g_list_ref, &stats);

    char resp_body[2048];
    int offset = snprintf(resp_body, sizeof(resp_body),
        "{"
        "\"status\":\"success\","
        "\"total\":%d,"
        "\"capacity\":%d,"
        "\"remaining\":%d,"
        "\"events\":[",
        stats.total,
        MAX_PARTICIPANTS,
        MAX_PARTICIPANTS - stats.total);

    for (int i = 0; i < NUM_EVENTS; i++) {
        offset += snprintf(resp_body + offset, sizeof(resp_body) - offset,
            "%s{\"id\":%d,\"name\":\"%s\",\"code\":\"%s\",\"count\":%d,\"percentage\":%.2f}",
            (i > 0 ? "," : ""),
            i,
            get_event_name(i),
            get_event_code(i),
            stats.event_counts[i],
            stats.event_percentages[i]);
    }

    snprintf(resp_body + offset, sizeof(resp_body) - offset, "]}");
    json_send_response(sock, 200, "OK", resp_body);
}

// Handler: GET /api/admin/participants
static void handle_get_admin_participants(SOCKET sock, const char *req, const char *query) {
    char token[64];
    if (!extract_admin_token(req, query, token, sizeof(token)) || !service_validate_token(token)) {
        json_send_error(sock, 401, "Unauthorized: Valid admin token required.");
        return;
    }

    char search_term[128] = "";
    char event_str[32] = "";
    int event_filter = -1;

    if (query) {
        get_query_param(query, "search", search_term, sizeof(search_term));
        if (get_query_param(query, "event", event_str, sizeof(event_str))) {
            event_filter = atoi(event_str);
        }
    }

    // Allocate results array on heap
    Participant *results = (Participant *)malloc(sizeof(Participant) * MAX_PARTICIPANTS);
    if (!results) {
        json_send_error(sock, 500, "Server memory allocation error");
        return;
    }

    int matched_count = service_search_and_filter(g_list_ref, search_term, event_filter, results, MAX_PARTICIPANTS);

    // Allocate JSON buffer (256 KB)
    size_t json_capacity = 256 * 1024;
    char *json_buf = (char *)malloc(json_capacity);
    if (!json_buf) {
        free(results);
        json_send_error(sock, 500, "Server memory allocation error");
        return;
    }

    int offset = snprintf(json_buf, json_capacity,
        "{\"status\":\"success\",\"count\":%d,\"total_registered\":%d,\"participants\":[",
        matched_count, g_list_ref->count);

    for (int i = 0; i < matched_count; i++) {
        const Participant *p = &results[i];
        char clean_name[MAX_NAME_LEN * 2];
        char clean_dept[MAX_DEPT_LEN * 2];
        json_escape(p->name, clean_name, sizeof(clean_name));
        json_escape(p->department, clean_dept, sizeof(clean_dept));

        int written = snprintf(json_buf + offset, json_capacity - offset,
            "%s{\"confirmation_no\":\"%s\",\"reg_id\":\"%s\",\"name\":\"%s\",\"department\":\"%s\",\"event_id\":%d,\"event_name\":\"%s\",\"event_code\":\"%s\",\"registered_at\":\"%s\"}",
            (i > 0 ? "," : ""),
            p->confirmation_no,
            p->reg_id,
            clean_name,
            clean_dept,
            p->event_id,
            get_event_name(p->event_id),
            get_event_code(p->event_id),
            p->registered_at);

        if (written > 0 && offset + written < (int)json_capacity - 100) {
            offset += written;
        } else {
            break;
        }
    }

    snprintf(json_buf + offset, json_capacity - offset, "]}");

    json_send_response(sock, 200, "OK", json_buf);

    free(json_buf);
    free(results);
}

// Handler: POST /api/admin/generate-summary or /api/admin/summary
static void handle_generate_summary(SOCKET sock, const char *req, const char *query) {
    char token[64];
    if (!extract_admin_token(req, query, token, sizeof(token)) || !service_validate_token(token)) {
        json_send_error(sock, 401, "Unauthorized: Valid admin token required.");
        return;
    }

    if (storage_generate_summary(SUMMARY_FILE, g_list_ref)) {
        char resp[512];
        snprintf(resp, sizeof(resp),
            "{\"status\":\"success\",\"message\":\"Summary report generated successfully in data/event_summary.txt\",\"file\":\"%s\"}",
            SUMMARY_FILE);
        json_send_response(sock, 200, "OK", resp);
    } else {
        json_send_error(sock, 500, "Failed to write event summary report.");
    }
}

// Handler: GET /api/admin/summary
static void handle_get_summary(SOCKET sock, const char *req, const char *query) {
    char token[64];
    if (!extract_admin_token(req, query, token, sizeof(token)) || !service_validate_token(token)) {
        json_send_error(sock, 401, "Unauthorized: Valid admin token required.");
        return;
    }

    FILE *f = fopen(SUMMARY_FILE, "r");
    if (!f) {
        // Auto-generate if not yet generated
        storage_generate_summary(SUMMARY_FILE, g_list_ref);
        f = fopen(SUMMARY_FILE, "r");
    }

    if (!f) {
        json_send_error(sock, 404, "Summary file not found.");
        return;
    }

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    char *text = (char *)malloc(size + 1);
    if (!text) {
        fclose(f);
        json_send_error(sock, 500, "Memory allocation failed");
        return;
    }

    fread(text, 1, size, f);
    text[size] = '\0';
    fclose(f);

    // Escape and send in JSON
    char *escaped = (char *)malloc((size * 2) + 1024);
    if (!escaped) {
        free(text);
        json_send_error(sock, 500, "Memory allocation failed");
        return;
    }

    json_escape(text, escaped, (size * 2) + 1024);
    free(text);

    char *json_resp = (char *)malloc((size * 2) + 2048);
    if (json_resp) {
        snprintf(json_resp, (size * 2) + 2048, "{\"status\":\"success\",\"content\":\"%s\"}", escaped);
        json_send_response(sock, 200, "OK", json_resp);
        free(json_resp);
    } else {
        json_send_error(sock, 500, "Memory allocation failed");
    }
    free(escaped);
}

// Main HTTP request router
static void route_request(SOCKET client_sock, const char *req_buf) {
    char method[16] = "";
    char full_path[256] = "";
    char http_version[16] = "";

    if (sscanf(req_buf, "%15s %255s %15s", method, full_path, http_version) < 2) {
        json_send_error(client_sock, 400, "Malformed HTTP request line.");
        return;
    }

    // Split path and query parameters
    char path[256] = "";
    char *query = NULL;
    char *qmark = strchr(full_path, '?');
    if (qmark) {
        *qmark = '\0';
        strncpy(path, full_path, sizeof(path) - 1);
        query = qmark + 1;
    } else {
        strncpy(path, full_path, sizeof(path) - 1);
    }

    printf(" [HTTP %s] %s\n", method, path);
    fflush(stdout);

    // Locate request body (after \r\n\r\n)
    const char *body = strstr(req_buf, "\r\n\r\n");
    if (body) {
        body += 4;
    } else {
        body = "";
    }

    // Handle OPTIONS (CORS preflight)
    if (strcmp(method, "OPTIONS") == 0) {
        handle_options(client_sock);
        return;
    }

    // Routing table
    if (strcmp(method, "GET") == 0) {
        if (strcmp(path, "/api/events") == 0) {
            handle_get_events(client_sock);
        } else if (strcmp(path, "/api/admin/stats") == 0) {
            handle_get_admin_stats(client_sock, req_buf, query);
        } else if (strcmp(path, "/api/admin/participants") == 0) {
            handle_get_admin_participants(client_sock, req_buf, query);
        } else if (strcmp(path, "/api/admin/summary") == 0) {
            handle_get_summary(client_sock, req_buf, query);
        } else {
            // Static file serving
            serve_static_file(client_sock, path);
        }
    } else if (strcmp(method, "POST") == 0) {
        if (strcmp(path, "/api/register") == 0) {
            handle_post_register(client_sock, body);
        } else if (strcmp(path, "/api/admin/login") == 0) {
            handle_post_admin_login(client_sock, body);
        } else if (strcmp(path, "/api/admin/summary") == 0 ||
                   strcmp(path, "/api/admin/generate-summary") == 0) {
            handle_generate_summary(client_sock, req_buf, query);
        } else {
            json_send_error(client_sock, 404, "Endpoint Not Found");
        }
    } else {
        json_send_error(client_sock, 405, "Method Not Allowed");
    }
}

int http_server_start(int port, ParticipantList *list) {
    g_list_ref = list;

    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        fprintf(stderr, "Error: WSAStartup failed with error: %d\n", WSAGetLastError());
        return 0;
    }

    SOCKET server_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (server_sock == INVALID_SOCKET) {
        fprintf(stderr, "Error: Could not create socket: %d\n", WSAGetLastError());
        WSACleanup();
        return 0;
    }

    // Allow address reuse
    int opt = 1;
    setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, (const char *)&opt, sizeof(opt));

    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons((u_short)port);

    if (bind(server_sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) == SOCKET_ERROR) {
        fprintf(stderr, "Error: Bind failed on port %d with error: %d\n", port, WSAGetLastError());
        closesocket(server_sock);
        WSACleanup();
        return 0;
    }

    if (listen(server_sock, 20) == SOCKET_ERROR) {
        fprintf(stderr, "Error: Listen failed with error: %d\n", WSAGetLastError());
        closesocket(server_sock);
        WSACleanup();
        return 0;
    }

    printf("\n====================================================================\n");
    printf("   CAMPUS EVENT REGISTRATION REGISTER - PURE C BACKEND HTTP SERVER \n");
    printf("====================================================================\n");
    printf(" [Server Status] : RUNNING\n");
    printf(" [Host / Port]   : http://localhost:%d\n", port);
    printf(" [Capacity Limit]: %d participants maximum\n", MAX_PARTICIPANTS);
    printf(" [Current Count ]: %d records loaded from %s\n", list->count, PARTICIPANTS_FILE);
    printf(" [Admin Login]   : Username: admin | Password: admin123\n");
    printf("====================================================================\n");
    printf(" Ready to accept student registrations and admin requests...\n\n");
    fflush(stdout);

    char *req_buf = (char *)malloc(REQ_BUF_SIZE);
    if (!req_buf) {
        fprintf(stderr, "Error: Failed to allocate request buffer.\n");
        closesocket(server_sock);
        WSACleanup();
        return 0;
    }

    while (1) {
        struct sockaddr_in client_addr;
        int client_len = sizeof(client_addr);
        SOCKET client_sock = accept(server_sock, (struct sockaddr *)&client_addr, &client_len);

        if (client_sock == INVALID_SOCKET) {
            continue;
        }

        memset(req_buf, 0, REQ_BUF_SIZE);
        int bytes_received = recv(client_sock, req_buf, REQ_BUF_SIZE - 1, 0);

        if (bytes_received > 0) {
            req_buf[bytes_received] = '\0';

            // Handle Expect: 100-continue for HTTP/1.1 clients
            if (strstr(req_buf, "100-continue") != NULL) {
                const char *cont = "HTTP/1.1 100 Continue\r\n\r\n";
                send(client_sock, cont, (int)strlen(cont), 0);
            }

            // Check Content-Length to ensure complete body is received
            char cl_str[32];
            if (get_header_value(req_buf, "Content-Length", cl_str, sizeof(cl_str))) {
                int content_len = atoi(cl_str);
                const char *body_start = strstr(req_buf, "\r\n\r\n");
                if (body_start) {
                    body_start += 4;
                    int body_received = bytes_received - (int)(body_start - req_buf);
                    while (body_received < content_len && bytes_received < REQ_BUF_SIZE - 1) {
                        int more = recv(client_sock, req_buf + bytes_received, REQ_BUF_SIZE - 1 - bytes_received, 0);
                        if (more <= 0) break;
                        bytes_received += more;
                        req_buf[bytes_received] = '\0';
                        body_received += more;
                    }
                }
            }

            route_request(client_sock, req_buf);
        }

        // Gracefully shutdown send channel before closing socket
        shutdown(client_sock, SD_SEND);
        closesocket(client_sock);
    }

    free(req_buf);
    closesocket(server_sock);
    WSACleanup();
    return 1;
}
