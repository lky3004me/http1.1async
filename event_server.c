#include <event2/event.h>
#include <event2/bufferevent.h>
#include <event2/listener.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <arpa/inet.h>

#define PORT 8080

void read_cb(struct bufferevent* bev, void* ctx) {
    char buffer[1024];
    size_t n = bufferevent_read(bev, buffer, sizeof(buffer) - 1);
    buffer[n] = '\0';

    printf("Client request:\n%s\n", buffer);

    const char* body = "<h1>Hello, libevent!</h1>";
    char response[1024];

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

    printf("Libevent server running at http://localhost:%d\n", PORT);
    event_base_dispatch(base);
    return 0;
}
