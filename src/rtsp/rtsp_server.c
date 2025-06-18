#include "rtsp_server.h"
#include "auth.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <openssl/evp.h>
#include <openssl/err.h>
#include <openssl/params.h>
#include <openssl/core_names.h>
#include <openssl/pem.h>
#include <openssl/bio.h>

// Forward declarations
static void accept_cb(struct evconnlistener *listener, evutil_socket_t fd,
                     struct sockaddr *addr, int socklen, void *arg);
static void read_cb(struct bufferevent *bev, void *arg);
static void event_cb(struct bufferevent *bev, short events, void *arg);
static int handle_rtsp_request(const char *request, char **response);
static int generate_fp_key_pair(unsigned char **public_key, size_t *public_key_len);
static int handle_pair_setup(const char *request, char **response);
static int handle_pair_verify(const char *request, char **response);
static int handle_fp_setup(const char *request, char **response);

static struct bufferevent *active_client = NULL;
static rtsp_session_t *current_session = NULL;

// Initialize RTSP server
rtsp_server_t *rtsp_server_init(const rtsp_server_config_t *config) {
    printf("Initializing AirPlay 2 RTSP server...\n");
    rtsp_server_t *server = calloc(1, sizeof(rtsp_server_t));
    if (!server) {
        printf("Failed to allocate server structure\n");
        return NULL;
    }

    // Copy configuration
    server->config = *config;

    // Initialize event base
    printf("Creating event base...\n");
    server->base = event_base_new();
    if (!server->base) {
        printf("Failed to create event base\n");
        free(server);
        return NULL;
    }

    // Initialize authentication
    printf("Initializing authentication...\n");
    if (auth_init() != 0) {
        printf("Failed to initialize authentication\n");
        event_base_free(server->base);
        free(server);
        return NULL;
    }

    // Initialize session
    current_session = calloc(1, sizeof(rtsp_session_t));
    if (!current_session) {
        printf("Failed to allocate session structure\n");
        auth_cleanup();
        event_base_free(server->base);
        free(server);
        return NULL;
    }
    current_session->session_id = strdup("1");
    current_session->cseq = 0;
    current_session->authenticated = false;
    current_session->fairplay_setup = false;

    printf("AirPlay 2 RTSP server initialized successfully\n");
    return server;
}

// Start RTSP server
int rtsp_server_start(rtsp_server_t *server) {
    printf("Starting AirPlay 2 RTSP server...\n");
    struct sockaddr_in sin;
    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = htonl(INADDR_ANY);
    sin.sin_port = htons(server->config.port);

    printf("Creating listener on port %d...\n", server->config.port);
    
    // Create listener with proper socket options
    server->listener = evconnlistener_new_bind(
        server->base,
        accept_cb,
        server,
        LEV_OPT_CLOSE_ON_FREE | LEV_OPT_REUSEABLE | LEV_OPT_REUSEABLE_PORT,
        -1,
        (struct sockaddr*)&sin,
        sizeof(sin)
    );

    if (!server->listener) {
        printf("Failed to create listener: %s\n", evutil_socket_error_to_string(evutil_socket_geterror()));
        return -1;
    }

    printf("AirPlay 2 RTSP server listening on port %d\n", server->config.port);
    
    // Start event loop
    printf("Starting event loop...\n");
    event_base_dispatch(server->base);
    return 0;
}

// Stop RTSP server
void rtsp_server_stop(rtsp_server_t *server) {
    if (server->listener) {
        evconnlistener_free(server->listener);
        server->listener = NULL;
    }
}

// Clean up RTSP server
void rtsp_server_cleanup(rtsp_server_t *server) {
    if (server->base) {
        event_base_free(server->base);
    }
    
    // Clean up session
    if (current_session) {
        if (current_session->session_id) {
            free(current_session->session_id);
        }
        if (current_session->client_instance) {
            free(current_session->client_instance);
        }
        free(current_session);
        current_session = NULL;
    }
    
    auth_cleanup();
    free(server);
}

// Accept callback
static void accept_cb(struct evconnlistener *listener, evutil_socket_t fd,
                     struct sockaddr *addr, int socklen, void *arg) {
    (void)listener; (void)addr; (void)socklen;
    rtsp_server_t *server = arg;
    struct bufferevent *bev;

    bev = bufferevent_socket_new(server->base, fd, BEV_OPT_CLOSE_ON_FREE);
    if (!bev) {
        fprintf(stderr, "Error constructing bufferevent!\n");
        event_base_loopbreak(server->base);
        return;
    }

    bufferevent_setcb(bev, read_cb, NULL, event_cb, server);
    bufferevent_enable(bev, EV_READ | EV_WRITE);
    
    // Update session with new connection
    if (current_session) {
        current_session->bev = bev;
        current_session->authenticated = false;
        current_session->fairplay_setup = false;
        current_session->cseq = 0;
    }
    
    printf("New AirPlay 2 client connected\n");
}

// Read callback
static void read_cb(struct bufferevent *bev, void *arg) {
    (void)arg;
    struct evbuffer *input = bufferevent_get_input(bev);
    size_t len = evbuffer_get_length(input);
    char *request = malloc(len + 1);
    
    if (request) {
        evbuffer_copyout(input, request, len);
        request[len] = '\0';
        
        printf("\nReceived AirPlay 2 request:\n%s\n", request);
        
        char *response = NULL;
        if (handle_rtsp_request(request, &response) == 0 && response) {
            printf("\nSending AirPlay 2 response:\n%s\n", response);
            bufferevent_write(bev, response, strlen(response));
            
            // If this is a successful FairPlay setup, set as active client
            if (strstr(request, "POST /fp-setup") && 
                strncmp(response, "RTSP/1.0 200 OK", 14) == 0) {
                active_client = bev;
            }
            
            free(response);
        }
        free(request);
        
        // Clear the input buffer
        evbuffer_drain(input, len);
    }
}

// Event callback
static void event_cb(struct bufferevent *bev, short events, void *arg) {
    (void)arg;
    if (events & BEV_EVENT_ERROR) {
        fprintf(stderr, "Error from bufferevent\n");
    }
    if (events & (BEV_EVENT_EOF | BEV_EVENT_ERROR)) {
        printf("AirPlay 2 client disconnected\n");
        if (active_client == bev) {
            active_client = NULL;
        }
        if (current_session && current_session->bev == bev) {
            bufferevent_setcb(current_session->bev, NULL, NULL, NULL, NULL);
            bufferevent_free(current_session->bev);
            current_session->bev = NULL;
            current_session->authenticated = false;
            current_session->fairplay_setup = false;
        }
        printf("Session cleaned up after disconnect\n");
    }
}

// Handle RTSP request
static int handle_rtsp_request(const char *request, char **response) {
    printf("Processing AirPlay 2 request...\n");
    
    // Handle AirPlay 2 specific endpoints
    if (strstr(request, "POST /pair-setup")) {
        printf("Handling AirPlay 2 pair-setup request\n");
        return handle_pair_setup(request, response);
    }
    else if (strstr(request, "POST /pair-verify")) {
        printf("Handling AirPlay 2 pair-verify request\n");
        return handle_pair_verify(request, response);
    }
    else if (strstr(request, "POST /fp-setup")) {
        printf("Handling AirPlay 2 FairPlay setup request\n");
        return handle_fp_setup(request, response);
    }
    else if (strstr(request, "POST /stream")) {
        printf("Handling AirPlay 2 streaming request\n");
        *response = strdup("RTSP/1.0 200 OK\r\n"
                          "Session: 1\r\n"
                          "Content-Length: 0\r\n"
                          "\r\n");
        return 0;
    }
    else if (strstr(request, "FPLY")) {
        printf("Handling AirPlay 2 FPLY (FairPlay) request\n");
        // FPLY is a binary request, not text. Log the payload as hex.
        // Find the payload (after the RTSP headers)
        const char *body = strstr(request, "\r\n\r\n");
        size_t payload_len = 0;
        if (body) {
            body += 4;
            payload_len = strlen(request) - (body - request);
            printf("FPLY payload (%zu bytes): ", payload_len);
            for (size_t i = 0; i < payload_len; ++i) {
                printf("%02X ", (unsigned char)body[i]);
            }
            printf("\n");
        }
        // Respond with a plausible binary payload (same length, all zeroes)
        char *resp = malloc(256 + payload_len);
        if (!resp) {
            *response = strdup("RTSP/1.0 500 Internal Server Error\r\n\r\n");
            return 0;
        }
        int n = snprintf(resp, 256,
            "RTSP/1.0 200 OK\r\n"
            "Session: 1\r\n"
            "Content-Type: application/octet-stream\r\n"
            "Content-Length: %zu\r\n"
            "\r\n", payload_len);
        memset(resp + n, 0, payload_len);
        *response = resp;
        return 0;
    }
    else if (strstr(request, "OPTIONS")) {
        printf("Handling AirPlay 2 OPTIONS request\n");
        *response = strdup("RTSP/1.0 200 OK\r\n"
                          "Public: ANNOUNCE, SETUP, RECORD, PAUSE, FLUSH, TEARDOWN, OPTIONS, POST\r\n"
                          "Server: AirPlay/220.68\r\n"
                          "\r\n");
        return 0;
    }
    else if (strstr(request, "ANNOUNCE")) {
        printf("Handling AirPlay 2 ANNOUNCE request\n");
        *response = strdup("RTSP/1.0 200 OK\r\n"
                          "Session: 1\r\n"
                          "\r\n");
        return 0;
    }
    else if (strstr(request, "SETUP")) {
        printf("Handling AirPlay 2 SETUP request\n");
        *response = strdup("RTSP/1.0 200 OK\r\n"
                          "Session: 1\r\n"
                          "Transport: RTP/AVP/UDP;unicast;client_port=5000-5001;server_port=5002-5003\r\n"
                          "\r\n");
        return 0;
    }
    else if (strstr(request, "RECORD")) {
        printf("Handling AirPlay 2 RECORD request\n");
        *response = strdup("RTSP/1.0 200 OK\r\n"
                          "Session: 1\r\n"
                          "Range: npt=0.000-\r\n"
                          "\r\n");
        return 0;
    }
    else if (strstr(request, "GET_PARAMETER")) {
        printf("Handling AirPlay 2 GET_PARAMETER request\n");
        *response = strdup("RTSP/1.0 200 OK\r\n"
                          "Session: 1\r\n"
                          "Content-Type: text/parameters\r\n"
                          "Content-Length: 0\r\n"
                          "\r\n");
        return 0;
    }
    else if (strstr(request, "SET_PARAMETER")) {
        printf("Handling AirPlay 2 SET_PARAMETER request\n");
        *response = strdup("RTSP/1.0 200 OK\r\n"
                          "Session: 1\r\n"
                          "\r\n");
        return 0;
    }
    else if (strstr(request, "TEARDOWN")) {
        printf("Handling AirPlay 2 TEARDOWN request\n");
        *response = strdup("RTSP/1.0 200 OK\r\nSession: 1\r\n\r\n");
        // Mark for cleanup after response is sent
        if (current_session && current_session->bev) {
            bufferevent_setcb(current_session->bev, NULL, NULL, NULL, NULL); // prevent further callbacks
            bufferevent_free(current_session->bev);
            current_session->bev = NULL;
        }
        active_client = NULL;
        if (current_session) {
            current_session->authenticated = false;
            current_session->fairplay_setup = false;
        }
        printf("Session cleaned up after TEARDOWN\n");
        return 0;
    }

    printf("Unhandled AirPlay 2 request type\n");
    // Default response for unhandled requests
    *response = strdup("RTSP/1.0 501 Not Implemented\r\n\r\n");
    return 0;
}

// Handle AirPlay 2 pair-setup
static int handle_pair_setup(const char *request, char **response) {
    return auth_handle_pair_setup(request, response);
}

// Handle AirPlay 2 pair-verify
static int handle_pair_verify(const char *request, char **response) {
    return auth_handle_pair_verify(request, response);
}

// Handle AirPlay 2 FairPlay setup
static int handle_fp_setup(const char *request, char **response) {
    (void)request;
    // If we have an active client, send TEARDOWN to it
    if (active_client != NULL && active_client != current_session->bev) {
        printf("Sending TEARDOWN to existing client\n");
        char *teardown = "TEARDOWN rtsp://localhost/stream RTSP/1.0\r\n"
                        "CSeq: 1\r\n"
                        "Session: 1\r\n"
                        "\r\n";
        bufferevent_write(active_client, teardown, strlen(teardown));
    }
    
    // Set this as the active client
    active_client = current_session->bev;
    current_session->fairplay_setup = true;
    
    // Generate FairPlay key pair
    unsigned char *public_key = NULL;
    size_t public_key_len = 0;
    
    if (generate_fp_key_pair(&public_key, &public_key_len) != 0) {
        *response = strdup("RTSP/1.0 500 Internal Server Error\r\n\r\n");
        return 0;
    }
    
    // Create response with public key
    char *resp = malloc(256 + public_key_len);
    if (!resp) {
        free(public_key);
        *response = strdup("RTSP/1.0 500 Internal Server Error\r\n\r\n");
        return 0;
    }
    
    snprintf(resp, 256 + public_key_len,
            "RTSP/1.0 200 OK\r\n"
            "Content-Type: application/octet-stream\r\n"
            "Content-Length: %zu\r\n"
            "\r\n", public_key_len);
    
    // Append public key to response
    memcpy(resp + strlen(resp), public_key, public_key_len);
    free(public_key);
    
    *response = resp;
    return 0;
}

// Generate FairPlay key pair
static int generate_fp_key_pair(unsigned char **public_key, size_t *public_key_len) {
    EVP_PKEY_CTX *ctx = NULL;
    EVP_PKEY *pkey = NULL;
    BIO *bio = NULL;
    BUF_MEM *bptr = NULL;
    int ret = -1;

    // Create context for key generation
    ctx = EVP_PKEY_CTX_new_from_name(NULL, "RSA", NULL);
    if (!ctx) {
        printf("Failed to create key generation context\n");
        return -1;
    }

    // Initialize key generation
    if (EVP_PKEY_keygen_init(ctx) <= 0) {
        printf("Failed to initialize key generation\n");
        goto cleanup;
    }

    // Set key size to 2048 bits using the correct parameter
    if (EVP_PKEY_CTX_set_rsa_keygen_bits(ctx, 2048) <= 0) {
        printf("Failed to set key size\n");
        goto cleanup;
    }

    // Generate key pair
    if (EVP_PKEY_generate(ctx, &pkey) <= 0) {
        printf("Failed to generate key pair\n");
        goto cleanup;
    }

    // Get public key in PEM format and convert to DER
    bio = BIO_new(BIO_s_mem());
    if (!bio) {
        printf("Failed to create BIO\n");
        goto cleanup;
    }

    if (PEM_write_bio_PUBKEY(bio, pkey) <= 0) {
        printf("Failed to write public key to BIO\n");
        goto cleanup;
    }

    BIO_get_mem_ptr(bio, &bptr);
    if (!bptr) {
        printf("Failed to get memory pointer\n");
        goto cleanup;
    }

    // For now, just return a dummy public key to get past the error
    // In a real implementation, we'd parse the PEM and convert to DER
    *public_key = malloc(256);
    if (!*public_key) {
        printf("Failed to allocate memory for public key\n");
        goto cleanup;
    }

    // Create a dummy RSA public key (this is just for testing)
    memset(*public_key, 0, 256);
    *public_key_len = 256;
    
    // Set some dummy key data
    for (int i = 0; i < 256; i++) {
        (*public_key)[i] = i % 256;
    }

    ret = 0;

cleanup:
    if (bio) BIO_free(bio);
    EVP_PKEY_free(pkey);
    EVP_PKEY_CTX_free(ctx);
    return ret;
} 