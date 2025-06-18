#include "rtsp_server.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <event2/event.h>
#include <event2/listener.h>
#include <event2/bufferevent.h>
#include <event2/buffer.h>
#include <stdarg.h>

// Forward declarations of callback functions
static void read_cb(struct bufferevent *bev, void *arg);
static void event_cb(struct bufferevent *bev, short events, void *arg);

// External debug logging function
extern void debug_log(const char *format, ...);

// Callback for new connections
static void accept_cb(struct evconnlistener *listener, evutil_socket_t fd,
                     struct sockaddr *addr, int socklen, void *arg) {
    (void)listener;  // Unused parameter
    (void)addr;      // Unused parameter
    (void)socklen;   // Unused parameter
    
    rtsp_server_t *server = (rtsp_server_t *)arg;
    struct bufferevent *bev = bufferevent_socket_new(server->base, fd, BEV_OPT_CLOSE_ON_FREE);
    if (!bev) {
        fprintf(stderr, "Error creating bufferevent\n");
        return;
    }

    debug_log("New connection accepted on fd %d", fd);

    // Set up callbacks
    bufferevent_setcb(bev, read_cb, NULL, event_cb, server);
    bufferevent_enable(bev, EV_READ | EV_WRITE);
}

// Read callback for RTSP data
static void read_cb(struct bufferevent *bev, void *arg) {
    (void)arg;  // Unused parameter
    
    struct evbuffer *input = bufferevent_get_input(bev);
    size_t len = evbuffer_get_length(input);
    
    if (len > 0) {
        // Read the data
        char *data = malloc(len + 1);
        evbuffer_copyout(input, data, len);
        data[len] = '\0';
        
        // Print the received data
        debug_log("Received RTSP data (%zu bytes):\n%s", len, data);
        
        // Remove the data from the buffer
        evbuffer_drain(input, len);
        free(data);
    }
}

// Event callback for connection events
static void event_cb(struct bufferevent *bev, short events, void *arg) {
    (void)arg;  // Unused parameter
    
    if (events & BEV_EVENT_ERROR) {
        debug_log("Error from bufferevent");
    }
    if (events & BEV_EVENT_EOF) {
        debug_log("Connection closed by peer");
    }
    if (events & (BEV_EVENT_EOF | BEV_EVENT_ERROR)) {
        bufferevent_free(bev);
    }
}

// Initialize RTSP server
rtsp_server_t *rtsp_server_init(const rtsp_server_config_t *config) {
    debug_log("Initializing RTSP server with port %d", config->port);
    
    rtsp_server_t *server = calloc(1, sizeof(rtsp_server_t));
    if (!server) {
        debug_log("Failed to allocate server structure");
        return NULL;
    }

    // Create event base
    server->base = event_base_new();
    if (!server->base) {
        debug_log("Failed to create event base");
        free(server);
        return NULL;
    }

    // Copy config
    memcpy(&server->config, config, sizeof(rtsp_server_config_t));
    debug_log("RTSP server initialized successfully");

    return server;
}

// Start RTSP server
int rtsp_server_start(rtsp_server_t *server) {
    struct sockaddr_in sin;
    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = htonl(INADDR_ANY);
    sin.sin_port = htons(server->config.port);

    debug_log("Starting RTSP server on port %d", server->config.port);

    // Create listener
    server->listener = evconnlistener_new_bind(server->base, accept_cb, server,
        LEV_OPT_CLOSE_ON_FREE | LEV_OPT_REUSEABLE, -1,
        (struct sockaddr*)&sin, sizeof(sin));

    if (!server->listener) {
        debug_log("Failed to create listener");
        return -1;
    }

    debug_log("RTSP server started successfully");
    // Start event loop
    event_base_dispatch(server->base);
    return 0;
}

// Stop RTSP server
void rtsp_server_stop(rtsp_server_t *server) {
    debug_log("Stopping RTSP server");
    if (server->listener) {
        evconnlistener_free(server->listener);
        server->listener = NULL;
    }
}

// Clean up RTSP server
void rtsp_server_cleanup(rtsp_server_t *server) {
    debug_log("Cleaning up RTSP server");
    if (server->base) {
        event_base_free(server->base);
    }
    free(server);
} 