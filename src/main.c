#include "rtsp_server.h"
#include "mdns_avahi.h"
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>

static rtsp_server_t *server = NULL;
static bool verbose_mode = false;

// Debug logging function
void debug_log(const char *format, ...) {
    if (!verbose_mode) return;
    
    va_list args;
    va_start(args, format);
    fprintf(stderr, "[DEBUG] ");
    vfprintf(stderr, format, args);
    fprintf(stderr, "\n");
    va_end(args);
}

static void signal_handler(int sig) {
    (void)sig;  // Unused parameter
    debug_log("Received signal %d, shutting down...", sig);
    
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
        if (strcmp(argv[i], "-v") == 0) {
            verbose_mode = true;
            debug_log("Verbose mode enabled");
        } else if (strncmp(argv[i], "--output-dir=", 13) == 0) {
            strncpy(config.output_dir, argv[i] + 13, sizeof(config.output_dir) - 1);
            config.output_dir[sizeof(config.output_dir) - 1] = '\0';
            debug_log("Output directory set to: %s", config.output_dir);
        }
    }

    // Strip trailing slash from output_dir (unless it's just "." or "/")
    size_t len = strlen(config.output_dir);
    while (len > 1 && config.output_dir[len - 1] == '/' && strcmp(config.output_dir, "/") != 0 && strcmp(config.output_dir, ".") != 0) {
        config.output_dir[len - 1] = '\0';
        len--;
    }

    debug_log("Starting mDNS advertisement for AIRSINK on port %d", config.port);
    // Start mDNS advertisement
    if (mdns_avahi_start("AIRSINK", config.port) != 0) {
        fprintf(stderr, "Failed to start mDNS advertisement\n");
        return 1;
    }

    debug_log("Initializing RTSP server");
    // Initialize server
    server = rtsp_server_init(&config);
    if (!server) {
        fprintf(stderr, "Failed to initialize RTSP server\n");
        mdns_avahi_stop();
        return 1;
    }

    printf("Starting AirPlay sink on port %d...\n", config.port);
    printf("Writing audio to directory: %s\n", config.output_dir);
    if (verbose_mode) {
        printf("Verbose logging enabled\n");
    }

    debug_log("Starting RTSP server");
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