#include "rtsp_server.h"
#include "mdns_avahi.h"
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>

static volatile bool running = true;
static volatile bool force_exit = false;
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

void signal_handler(int signum) {
    if (signum == SIGINT) {
        if (force_exit) {
            debug_log("Force exit requested, terminating immediately");
            exit(1);
        }
        debug_log("Received signal %d, shutting down...", signum);
        running = false;
        force_exit = true;
    }
}

void print_usage(const char *prog_name) {
    printf("Usage: %s [-v] [-p port] [-d directory]\n", prog_name);
    printf("Options:\n");
    printf("  -v         Enable verbose logging\n");
    printf("  -p port    Specify port number (default: 7000)\n");
    printf("  -d dir     Specify output directory (default: current directory)\n");
    printf("  -h         Show this help message\n");
    printf("\n");
    printf("AirPlay 2 Audio Sink - Receives AirPlay 2 audio streams\n");
}

int main(int argc, char *argv[]) {
    int port = 7000;
    const char *output_dir = ".";
    bool verbose = false;
    int opt;

    // Parse command line arguments
    while ((opt = getopt(argc, argv, "p:o:vh")) != -1) {
        switch (opt) {
            case 'p':
                port = atoi(optarg);
                break;
            case 'o':
                output_dir = optarg;
                break;
            case 'v':
                verbose = true;
                verbose_mode = true;
                break;
            case 'h':
                print_usage(argv[0]);
                return 0;
            default:
                fprintf(stderr, "Usage: %s [-p port] [-o output_dir] [-v]\n", argv[0]);
                return 1;
        }
    }

    // Set up signal handlers
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;

    if (sigaction(SIGINT, &sa, NULL) == -1) {
        perror("Failed to set up signal handler");
        return 1;
    }

    // Prepare RTSP server config
    rtsp_server_config_t config;
    memset(&config, 0, sizeof(config));
    config.port = port;
    config.cert_path = NULL;
    config.key_path = NULL;
    strncpy(config.output_dir, output_dir, sizeof(config.output_dir) - 1);
    config.output_dir[sizeof(config.output_dir) - 1] = '\0';

    // Initialize and start mDNS advertisement
    debug_log("Starting mDNS advertisement for AirPlay 2 AIRSINK on port %d", config.port);
    if (mdns_avahi_init("AIRSINK", config.port, config.port) != 0) {
        fprintf(stderr, "Failed to initialize mDNS advertisement\n");
        return 1;
    }

    if (mdns_avahi_start("AIRSINK", config.port) != 0) {
        fprintf(stderr, "Failed to start mDNS advertisement\n");
        mdns_avahi_cleanup();
        return 1;
    }

    // Initialize RTSP server
    rtsp_server_t *server = rtsp_server_init(&config);
    if (!server) {
        fprintf(stderr, "Failed to initialize AirPlay 2 RTSP server\n");
        mdns_avahi_cleanup();
        return 1;
    }

    // Start RTSP server
    if (rtsp_server_start(server) != 0) {
        fprintf(stderr, "Failed to start AirPlay 2 RTSP server\n");
        rtsp_server_cleanup(server);
        mdns_avahi_cleanup();
        return 1;
    }

    printf("Starting AirPlay 2 sink on port %d...\n", port);
    printf("Writing audio to directory: %s\n", output_dir);
    if (verbose) {
        printf("Verbose logging enabled\n");
    }

    // Main loop
    while (running) {
        sleep(1);
    }

    // Cleanup
    debug_log("Stopping AirPlay 2 RTSP server");
    rtsp_server_stop(server);
    rtsp_server_cleanup(server);

    debug_log("Stopping mDNS advertisement");
    mdns_avahi_cleanup();

    return 0;
} 