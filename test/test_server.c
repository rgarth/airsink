#include "../include/rtsp_server.h"
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>

static rtsp_server_t *server = NULL;

static void signal_handler(int sig) {
    if (server) {
        printf("\nShutting down server...\n");
        rtsp_server_stop(server);
        rtsp_server_cleanup(server);
        server = NULL;
    }
    exit(0);
}

int main(void) {
    // Set up signal handlers
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    // Configure RTSP server
    rtsp_server_config_t config = {
        .port = 7000,  // Default AirPlay port
        .cert_path = NULL,
        .key_path = NULL,
        .output_dir = "." // Default to current directory
    };

    printf("Starting AirPlay sink test server on port %d...\n", config.port);
    printf("Press Ctrl+C to stop\n\n");

    // Initialize server
    server = rtsp_server_init(&config);
    if (!server) {
        fprintf(stderr, "Failed to initialize RTSP server\n");
        return 1;
    }

    // Start server
    if (rtsp_server_start(server) != 0) {
        fprintf(stderr, "Failed to start RTSP server\n");
        rtsp_server_cleanup(server);
        return 1;
    }

    return 0;
} 