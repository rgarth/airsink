#include "rtsp_server.h"
#include "mdns_avahi.h"
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>

static rtsp_server_t *server = NULL;

static void signal_handler(int sig) {
    if (server) {
        rtsp_server_stop(server);
        rtsp_server_cleanup(server);
        server = NULL;
    }
    mdns_avahi_stop();
    exit(0);
}

int main(int argc, char *argv[]) {
    // Set up signal handlers
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    // Configure RTSP server
    rtsp_server_config_t config = {
        .port = 7000,  // Default AirPlay port
        .cert_path = NULL,  // TODO: Add certificate paths
        .key_path = NULL
    };

    // Start mDNS advertisement
    if (mdns_avahi_start("AIRSINK", config.port) != 0) {
        fprintf(stderr, "Failed to start mDNS advertisement\n");
        return 1;
    }

    // Initialize server
    server = rtsp_server_init(&config);
    if (!server) {
        fprintf(stderr, "Failed to initialize RTSP server\n");
        mdns_avahi_stop();
        return 1;
    }

    printf("Starting AirPlay sink on port %d...\n", config.port);

    // Start server
    if (rtsp_server_start(server) != 0) {
        fprintf(stderr, "Failed to start RTSP server\n");
        rtsp_server_cleanup(server);
        mdns_avahi_stop();
        return 1;
    }

    mdns_avahi_stop();
    return 0;
} 