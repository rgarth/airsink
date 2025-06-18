#include "mdns_avahi.h"
#include <avahi-client/client.h>
#include <avahi-client/publish.h>
#include <avahi-common/error.h>
#include <avahi-common/thread-watch.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// External debug logging function
extern void debug_log(const char *format, ...);

static AvahiClient *client = NULL;
static AvahiEntryGroup *group = NULL;
static AvahiThreadedPoll *threaded_poll = NULL;

static void entry_group_callback(AvahiEntryGroup *g, AvahiEntryGroupState state, void *userdata) {
    (void)g;  // Unused parameter
    (void)userdata;  // Unused parameter

    switch (state) {
        case AVAHI_ENTRY_GROUP_ESTABLISHED:
            debug_log("mDNS service established");
            break;
        case AVAHI_ENTRY_GROUP_COLLISION:
            debug_log("mDNS service name collision");
            break;
        case AVAHI_ENTRY_GROUP_FAILURE:
            debug_log("mDNS service failed: %s", avahi_strerror(avahi_client_errno(client)));
            break;
        case AVAHI_ENTRY_GROUP_UNCOMMITED:
        case AVAHI_ENTRY_GROUP_REGISTERING:
            break;
    }
}

static void client_callback(AvahiClient *c, AvahiClientState state, void *userdata) {
    (void)userdata;  // Unused parameter

    switch (state) {
        case AVAHI_CLIENT_S_RUNNING:
            debug_log("Avahi client running");
            break;
        case AVAHI_CLIENT_FAILURE:
            debug_log("Avahi client failure: %s", avahi_strerror(avahi_client_errno(c)));
            break;
        case AVAHI_CLIENT_S_COLLISION:
        case AVAHI_CLIENT_S_REGISTERING:
            break;
        case AVAHI_CLIENT_CONNECTING:
            debug_log("Avahi client connecting");
            break;
    }
}

int mdns_avahi_start(const char *name, int port) {
    int error;

    debug_log("Starting mDNS advertisement for %s on port %d", name, port);

    // Create threaded poll object
    threaded_poll = avahi_threaded_poll_new();
    if (!threaded_poll) {
        debug_log("Failed to create threaded poll object");
        return -1;
    }

    // Create client
    client = avahi_client_new(avahi_threaded_poll_get(threaded_poll),
                            AVAHI_CLIENT_NO_FAIL,
                            client_callback,
                            NULL,
                            &error);
    if (!client) {
        debug_log("Failed to create Avahi client: %s", avahi_strerror(error));
        avahi_threaded_poll_free(threaded_poll);
        return -1;
    }

    // Create entry group
    group = avahi_entry_group_new(client, entry_group_callback, NULL);
    if (!group) {
        debug_log("Failed to create entry group: %s", avahi_strerror(avahi_client_errno(client)));
        avahi_client_free(client);
        avahi_threaded_poll_free(threaded_poll);
        return -1;
    }

    // Add service
    if (avahi_entry_group_add_service(group,
                                    AVAHI_IF_UNSPEC,
                                    AVAHI_PROTO_UNSPEC,
                                    0,
                                    name,
                                    "_airplay._tcp",
                                    NULL,
                                    NULL,
                                    port,
                                    "model=AIRSINK",
                                    "features=0x4A7FCA00,0x3C356BD0",
                                    "deviceid=00:11:22:33:44:55",
                                    "protovers=1.1",
                                    NULL) < 0) {
        debug_log("Failed to add service: %s", avahi_strerror(avahi_client_errno(client)));
        avahi_entry_group_free(group);
        avahi_client_free(client);
        avahi_threaded_poll_free(threaded_poll);
        return -1;
    }

    // Commit the entry group
    if (avahi_entry_group_commit(group) < 0) {
        debug_log("Failed to commit entry group: %s", avahi_strerror(avahi_client_errno(client)));
        avahi_entry_group_free(group);
        avahi_client_free(client);
        avahi_threaded_poll_free(threaded_poll);
        return -1;
    }

    debug_log("mDNS service advertisement started successfully");
    // Start the threaded poll
    avahi_threaded_poll_start(threaded_poll);

    return 0;
}

void mdns_avahi_stop(void) {
    debug_log("Stopping mDNS advertisement");
    if (group) {
        avahi_entry_group_free(group);
        group = NULL;
    }
    if (client) {
        avahi_client_free(client);
        client = NULL;
    }
    if (threaded_poll) {
        avahi_threaded_poll_stop(threaded_poll);
        avahi_threaded_poll_free(threaded_poll);
        threaded_poll = NULL;
    }
    debug_log("mDNS advertisement stopped");
} 