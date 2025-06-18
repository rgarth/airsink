#include "rtsp_server.h"
#include "auth.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

// Forward declarations
static void accept_cb(struct evconnlistener *listener, evutil_socket_t fd,
                     struct sockaddr *addr, int socklen, void *arg);
static void read_cb(struct bufferevent *bev, void *arg);
static void event_cb(struct bufferevent *bev, short events, void *arg);
static int handle_rtsp_request(const char *request, char **response);

// Initialize RTSP server
rtsp_server_t *rtsp_server_init(const rtsp_server_config_t *config) {
    rtsp_server_t *server = calloc(1, sizeof(rtsp_server_t));
    if (!server) return NULL;

    // Copy configuration
    server->config = *config;

    // Initialize event base
    server->base = event_base_new();
    if (!server->base) {
        free(server);
        return NULL;
    }

    // Initialize authentication
    if (auth_init() != 0) {
        event_base_free(server->base);
        free(server);
        return NULL;
    }

    return server;
}

// Start RTSP server
int rtsp_server_start(rtsp_server_t *server) {
    struct sockaddr_in sin;
    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = htonl(INADDR_ANY);
    sin.sin_port = htons(server->config.port);

    // Create listener
    server->listener = evconnlistener_new_bind(
        server->base,
        accept_cb,
        server,
        LEV_OPT_CLOSE_ON_FREE | LEV_OPT_REUSEABLE,
        -1,
        (struct sockaddr*)&sin,
        sizeof(sin)
    );

    if (!server->listener) {
        return -1;
    }

    printf("RTSP server listening on port %d\n", server->config.port);
    
    // Start event loop
    event_base_dispatch(server->base);
    return 0;
}

// Stop RTSP server
void rtsp_server_stop(rtsp_server_t *server) {
    if (server->listener) {
        evconnlistener_free(server->listener);
        server->listener = NULL;
    }
}

// Clean up RTSP server
void rtsp_server_cleanup(rtsp_server_t *server) {
    if (server->base) {
        event_base_free(server->base);
    }
    auth_cleanup();
    free(server);
}

// Accept callback
static void accept_cb(struct evconnlistener *listener, evutil_socket_t fd,
                     struct sockaddr *addr, int socklen, void *arg) {
    rtsp_server_t *server = arg;
    struct bufferevent *bev;

    bev = bufferevent_socket_new(server->base, fd, BEV_OPT_CLOSE_ON_FREE);
    if (!bev) {
        fprintf(stderr, "Error constructing bufferevent!\n");
        event_base_loopbreak(server->base);
        return;
    }

    bufferevent_setcb(bev, read_cb, NULL, event_cb, server);
    bufferevent_enable(bev, EV_READ | EV_WRITE);
    
    printf("New client connected\n");
}

// Read callback
static void read_cb(struct bufferevent *bev, void *arg) {
    struct evbuffer *input = bufferevent_get_input(bev);
    size_t len = evbuffer_get_length(input);
    char *request = malloc(len + 1);
    
    if (request) {
        evbuffer_copyout(input, request, len);
        request[len] = '\0';
        
        printf("\nReceived RTSP request:\n%s\n", request);
        
        char *response = NULL;
        if (handle_rtsp_request(request, &response) == 0 && response) {
            printf("\nSending RTSP response:\n%s\n", response);
            bufferevent_write(bev, response, strlen(response));
            free(response);
        }
        free(request);
        
        // Clear the input buffer
        evbuffer_drain(input, len);
    }
}

// Event callback
static void event_cb(struct bufferevent *bev, short events, void *arg) {
    if (events & BEV_EVENT_ERROR) {
        fprintf(stderr, "Error from bufferevent\n");
    }
    if (events & (BEV_EVENT_EOF | BEV_EVENT_ERROR)) {
        printf("Client disconnected\n");
        bufferevent_free(bev);
    }
}

// Handle RTSP request
static int handle_rtsp_request(const char *request, char **response) {
    printf("Processing RTSP request...\n");
    
    if (strncmp(request, "POST /pair-setup", 16) == 0) {
        printf("Handling pair-setup request\n");
        return auth_handle_pair_setup(request, response);
    }
    else if (strncmp(request, "POST /pair-verify", 17) == 0) {
        printf("Handling pair-verify request\n");
        return auth_handle_pair_verify(request, response);
    }
    else if (strncmp(request, "OPTIONS", 7) == 0) {
        printf("Handling OPTIONS request\n");
        *response = strdup("RTSP/1.0 200 OK\r\n"
                          "Public: ANNOUNCE, SETUP, RECORD, PAUSE, FLUSH, TEARDOWN, OPTIONS, GET_PARAMETER, SET_PARAMETER\r\n"
                          "\r\n");
        return 0;
    }
    else if (strncmp(request, "ANNOUNCE", 8) == 0) {
        printf("Handling ANNOUNCE request\n");
        *response = strdup("RTSP/1.0 200 OK\r\n\r\n");
        return 0;
    }
    else if (strncmp(request, "SETUP", 5) == 0) {
        printf("Handling SETUP request\n");
        *response = strdup("RTSP/1.0 200 OK\r\n"
                          "Session: 1\r\n"
                          "Transport: RTP/AVP/UDP;unicast;client_port=5000-5001;server_port=5002-5003\r\n"
                          "\r\n");
        return 0;
    }
    else if (strncmp(request, "RECORD", 6) == 0) {
        printf("Handling RECORD request\n");
        *response = strdup("RTSP/1.0 200 OK\r\n"
                          "Session: 1\r\n"
                          "Range: npt=0.000-\r\n"
                          "\r\n");
        return 0;
    }

    printf("Unhandled request type\n");
    // Default response for unhandled requests
    *response = strdup("RTSP/1.0 501 Not Implemented\r\n\r\n");
    return 0;
} 