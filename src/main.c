#include "rtsp_server.h"
#include "mdns_avahi.h"
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>

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
        .key_path = NULL,
        .output_dir = "." // Default to current directory
    };

    // Parse command-line arguments
    for (int i = 1; i < argc; ++i) {
        if (strncmp(argv[i], "--output-dir=", 13) == 0) {
            strncpy(config.output_dir, argv[i] + 13, sizeof(config.output_dir) - 1);
            config.output_dir[sizeof(config.output_dir) - 1] = '\0';
        }
    }

    // Strip trailing slash from output_dir (unless it's just "." or "/")
    size_t len = strlen(config.output_dir);
    while (len > 1 && config.output_dir[len - 1] == '/' && strcmp(config.output_dir, "/") != 0 && strcmp(config.output_dir, ".") != 0) {
        config.output_dir[len - 1] = '\0';
        len--;
    }

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
    printf("Writing audio to directory: %s\n", config.output_dir);

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