#ifndef RTSP_SERVER_H
#define RTSP_SERVER_H

#include <event2/event.h>
#include <event2/listener.h>
#include <event2/bufferevent.h>
#include <event2/buffer.h>
#include <openssl/ssl.h>

// RTSP server configuration
typedef struct {
    int port;
    const char *cert_path;
    const char *key_path;
    char output_dir[256]; // New: output directory for audio files
} rtsp_server_config_t;

// RTSP connection state
struct rtsp_connection {
    struct bufferevent *bev;
    SSL *ssl;
    char *session_id;
    int cseq;
};

// RTSP session state
typedef struct {
    char *session_id;
    int cseq;
    struct bufferevent *bev;
    SSL *ssl;
    bool authenticated;
    bool fairplay_setup;
    char *client_instance;
    // Add more session state as needed
} rtsp_session_t;

// RTSP server instance
typedef struct {
    struct event_base *base;
    struct evconnlistener *listener;
    rtsp_server_config_t config;
    // Add more server state as needed
} rtsp_server_t;

// Initialize RTSP server
rtsp_server_t *rtsp_server_init(const rtsp_server_config_t *config);

// Start RTSP server
int rtsp_server_start(rtsp_server_t *server);

// Stop RTSP server
void rtsp_server_stop(rtsp_server_t *server);

// Clean up RTSP server
void rtsp_server_cleanup(rtsp_server_t *server);

#endif // RTSP_SERVER_H 