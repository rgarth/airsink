#ifndef MDNS_AVAHI_H
#define MDNS_AVAHI_H

// Start mDNS advertisement for AirPlay (RAOP) service
int mdns_avahi_init(const char *service_name, int airplay_port, int raop_port);
int mdns_avahi_start(const char *name, int port);

// Stop mDNS advertisement
void mdns_avahi_stop(void);

void mdns_avahi_cleanup(void);

#endif // MDNS_AVAHI_H 