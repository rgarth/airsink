#ifndef AUTH_H
#define AUTH_H

#include <openssl/ssl.h>
#include <openssl/evp.h>

// Initialize authentication module
int auth_init(void);

// Clean up authentication module
void auth_cleanup(void);

// Handle pair-setup request
int auth_handle_pair_setup(const char *request, char **response);

// Handle pair-verify request
int auth_handle_pair_verify(const char *request, char **response);

// Get the AirPlay private key
EVP_PKEY *auth_get_private_key(void);

#endif // AUTH_H 