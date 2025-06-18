#include "mdns_avahi.h"
#include <avahi-client/client.h>
#include <avahi-client/publish.h>
#include <avahi-common/error.h>
#include <avahi-common/thread-watch.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static AvahiThreadedPoll *threaded_poll = NULL;
static AvahiClient *client = NULL;
static AvahiEntryGroup *group = NULL;

static void entry_group_callback(AvahiEntryGroup *g, AvahiEntryGroupState state, void *userdata) {
    (void)g; (void)userdata;
    if (state == AVAHI_ENTRY_GROUP_COLLISION) {
        fprintf(stderr, "mDNS name collision!\n");
    }
}

static void client_callback(AvahiClient *c, AvahiClientState state, void *userdata) {
    (void)userdata;
    if (state == AVAHI_CLIENT_FAILURE) {
        fprintf(stderr, "Avahi client failure: %s\n", avahi_strerror(avahi_client_errno(c)));
    }
}

int mdns_avahi_start(const char *name, int port) {
    int error;
    char hostname[256];
    snprintf(hostname, sizeof(hostname), "%s@%s", name, "AIRSINK");

    threaded_poll = avahi_threaded_poll_new();
    if (!threaded_poll) {
        fprintf(stderr, "Failed to create Avahi threaded poll\n");
        return -1;
    }

    client = avahi_client_new(avahi_threaded_poll_get(threaded_poll), 0, client_callback, NULL, &error);
    if (!client) {
        fprintf(stderr, "Failed to create Avahi client: %s\n", avahi_strerror(error));
        avahi_threaded_poll_free(threaded_poll);
        return -1;
    }

    group = avahi_entry_group_new(client, entry_group_callback, NULL);
    if (!group) {
        fprintf(stderr, "Failed to create Avahi entry group\n");
        avahi_client_free(client);
        avahi_threaded_poll_free(threaded_poll);
        return -1;
    }

    // Advertise as _airplay._tcp (AirPlay 2)
    char airplay_name[256];
    // Use proper AirPlay 2 format: MAC@DeviceName
    snprintf(airplay_name, sizeof(airplay_name), "485D607CEE22@%s", name);
    int ret = avahi_entry_group_add_service(
        group,
        AVAHI_IF_UNSPEC, AVAHI_PROTO_UNSPEC, 0,
        airplay_name,
        "_airplay._tcp",
        NULL, NULL,
        port,
        "deviceid=48:5D:60:7C:EE:22", 
        "features=0x5A7FFFF7,0x1E", 
        "model=AppleTV2,1", 
        "srcvers=220.68", 
        "protovers=1.0", 
        "pk=1234567890abcdef1234567890abcdef1234567890abcdef1234567890abcdef",
        "acl=0",
        "rsf=0x0",
        "ft=0x5A7FFFF7,0x1E",
        "vs=130.14",
        "tp=TCP,UDP",
        "md=0,1,2",
        "pw=false",
        "sr=44100",
        "ss=16",
        "ch=2",
        "cn=0,1",
        "et=0,1",
        "ek=1",
        "sf=0x4",
        "da=true",
        "sv=false",
        "sm=false",
        NULL
    );
    if (ret < 0) {
        fprintf(stderr, "Failed to add mDNS service: %s\n", avahi_strerror(ret));
        avahi_entry_group_free(group);
        avahi_client_free(client);
        avahi_threaded_poll_free(threaded_poll);
        return -1;
    }

    avahi_entry_group_commit(group);
    avahi_threaded_poll_start(threaded_poll);
    printf("mDNS: AirPlay 2 service '%s' advertised on port %d\n", airplay_name, port);
    return 0;
}

void mdns_avahi_stop(void) {
    if (threaded_poll) {
        avahi_threaded_poll_stop(threaded_poll);
    }
    if (group) {
        avahi_entry_group_free(group);
        group = NULL;
    }
    if (client) {
        avahi_client_free(client);
        client = NULL;
    }
    if (threaded_poll) {
        avahi_threaded_poll_free(threaded_poll);
        threaded_poll = NULL;
    }
}

int mdns_avahi_init(const char *service_name, int airplay_port, int raop_port) {
    // Stub: Assume initialization is handled in mdns_avahi_start for now
    (void)service_name;
    (void)airplay_port;
    (void)raop_port;
    return 0;
}

void mdns_avahi_cleanup(void) {
    mdns_avahi_stop();
} 