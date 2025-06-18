#ifndef MDNS_AVAHI_H
#define MDNS_AVAHI_H

// Start mDNS advertisement for AirPlay (RAOP) service
int mdns_avahi_start(const char *name, int port);

// Stop mDNS advertisement
void mdns_avahi_stop(void);

#endif // MDNS_AVAHI_H 