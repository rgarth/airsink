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
    if (state == AVAHI_ENTRY_GROUP_COLLISION) {
        fprintf(stderr, "mDNS name collision!\n");
    }
}

static void client_callback(AvahiClient *c, AvahiClientState state, void *userdata) {
    if (state == AVAHI_CLIENT_FAILURE) {
        fprintf(stderr, "Avahi client failure: %s\n", avahi_strerror(avahi_client_errno(c)));
    }
}

int mdns_avahi_start(const char *name, int port) {
    int error;
    char *service_name = NULL;
    char hostname[256];
    snprintf(hostname, sizeof(hostname), "%s@%s", name, "AIRSINK");
    service_name = hostname;

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

    // Advertise as _raop._tcp (AirPlay audio)
    char raop_name[256];
    snprintf(raop_name, sizeof(raop_name), "112233445566@%s", name); // MAC@Name
    int ret = avahi_entry_group_add_service(
        group,
        AVAHI_IF_UNSPEC, AVAHI_PROTO_UNSPEC, 0,
        raop_name,
        "_raop._tcp",
        NULL, NULL,
        port,
        "txtvers=1", "ch=2", "cn=0,1,2,3", "et=0,3,5", "sv=false", "da=true", "sr=44100", "ss=16", "pw=false", "vn=65537", "tp=UDP", NULL
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
    printf("mDNS: AirPlay service '%s' advertised on port %d\n", raop_name, port);
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