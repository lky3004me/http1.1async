#include <event2/event.h>
#include <event2/bufferevent.h>
#include <event2/listener.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sqlite3.h>
#include <arpa/inet.h>

#define PORT 8080

sqlite3* db;
char* list_body_ptr = NULL;

int callback(void* unused, int argc, char** argv, char** col_name) {
    list_body_ptr += sprintf(list_body_ptr, "<li>%s: %s</li>", argv[0], argv[1]);
    return 0;
}

void init_db() {
    char* err_msg = NULL;
    if (sqlite3_open("board.db", &db) != SQLITE_OK) {
        fprintf(stderr, "DB 열기 실패: %s\n", sqlite3_errmsg(db));
        exit(1);
    }

    const char* create_sql =
        "CREATE TABLE IF NOT EXISTS posts ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT, "
        "title TEXT NOT NULL);";

    if (sqlite3_exec(db, create_sql, 0, 0, &err_msg) != SQLITE_OK) {
        fprintf(stderr, "테이블 생성 실패: %s\n", err_msg);
        sqlite3_free(err_msg);
        exit(1);
    }
}

void read_cb(struct bufferevent* bev, void* ctx) {
    char buffer[4096];
    size_t n = bufferevent_read(bev, buffer, sizeof(buffer) - 1);
    buffer[n] = '\0';

    char body[2048] = "<h1>Not Found</h1>";
    char response[4096];

    if (strstr(buffer, "GET /add?title=")) {
        char* start = strstr(buffer, "GET /add?title=") + strlen("GET /add?title=");
        char* end = strchr(start, ' ');
        if (end) *end = '\0';

        char sql[512];
        char* err_msg = NULL;
        snprintf(sql, sizeof(sql), "INSERT INTO posts (title) VALUES ('%s');", start);
        if (sqlite3_exec(db, sql, 0, 0, &err_msg) == SQLITE_OK) {
            snprintf(body, sizeof(body), "<h1>Post Added: %s</h1>", start);
        } else {
            snprintf(body, sizeof(body), "<h1>DB Insert Error: %s</h1>", err_msg);
            sqlite3_free(err_msg);
        }

    } else if (strstr(buffer, "GET /list")) {
        char* err_msg = NULL;
        strcpy(body, "<h1>Post List</h1><ul>");
        list_body_ptr = body + strlen(body);

        const char* sql = "SELECT id, title FROM posts;";
        if (sqlite3_exec(db, sql, callback, NULL, &err_msg) != SQLITE_OK) {
            snprintf(body, sizeof(body), "<h1>DB Read Error: %s</h1>", err_msg);
            sqlite3_free(err_msg);
        } else {
            strcat(body, "</ul>");
        }
    }

    snprintf(response, sizeof(response),
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/html; charset=utf-8\r\n"
        "Content-Length: %zu\r\n"
        "\r\n"
        "%s", strlen(body), body);

    bufferevent_write(bev, response, strlen(response));
}

void event_cb(struct bufferevent* bev, short events, void* ctx) {
    if (events & (BEV_EVENT_EOF | BEV_EVENT_ERROR)) {
        bufferevent_free(bev);
    }
}

void accept_cb(struct evconnlistener* listener, evutil_socket_t fd,
               struct sockaddr* addr, int len, void* ctx) {
    struct event_base* base = evconnlistener_get_base(listener);
    struct bufferevent* bev = bufferevent_socket_new(base, fd, BEV_OPT_CLOSE_ON_FREE);

    bufferevent_setcb(bev, read_cb, NULL, event_cb, NULL);
    bufferevent_enable(bev, EV_READ | EV_WRITE);
}

int main() {
    init_db();

    struct event_base* base;
    struct evconnlistener* listener;
    struct sockaddr_in sin;

    base = event_base_new();
    if (!base) {
        puts("Could not initialize libevent!");
        return 1;
    }

    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = htonl(0);
    sin.sin_port = htons(PORT);

    listener = evconnlistener_new_bind(base, accept_cb, NULL,
        LEV_OPT_REUSEABLE | LEV_OPT_CLOSE_ON_FREE, -1,
        (struct sockaddr*)&sin, sizeof(sin));

    if (!listener) {
        perror("Could not create listener");
        return 1;
    }

    printf("Libevent + SQLite 서버 실행 중: http://localhost:%d\n", PORT);
    event_base_dispatch(base);
    
    sqlite3_close(db);
    return 0;
}
