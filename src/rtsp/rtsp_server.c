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

// Forward declarations
static void accept_cb(struct evconnlistener *listener, evutil_socket_t fd,
                     struct sockaddr *addr, int socklen, void *arg);
static void read_cb(struct bufferevent *bev, void *arg);
static void event_cb(struct bufferevent *bev, short events, void *arg);
static int handle_rtsp_request(const char *request, char **response);
static int generate_fp_key_pair(unsigned char **public_key, size_t *public_key_len);

// Initialize RTSP server
rtsp_server_t *rtsp_server_init(const rtsp_server_config_t *config) {
    printf("Initializing RTSP server...\n");
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

    printf("RTSP server initialized successfully\n");
    return server;
}

// Start RTSP server
int rtsp_server_start(rtsp_server_t *server) {
    printf("Starting RTSP server...\n");
    struct sockaddr_in sin;
    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = htonl(INADDR_ANY);
    sin.sin_port = htons(server->config.port);

    printf("Creating listener on port %d...\n", server->config.port);
    // Create listener
    server->listener = evconnlistener_new_bind(
        server->base,
        accept_cb,
        server,
        LEV_OPT_CLOSE_ON_FREE | LEV_OPT_REUSEABLE,
        -1,
        (struct sockaddr*)&sin,
        sizeof(sin)
    );

    if (!server->listener) {
        printf("Failed to create listener: %s\n", evutil_socket_error_to_string(evutil_socket_geterror()));
        return -1;
    }

    printf("RTSP server listening on port %d\n", server->config.port);
    
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
    auth_cleanup();
    free(server);
}

// Accept callback
static void accept_cb(struct evconnlistener *listener, evutil_socket_t fd,
                     struct sockaddr *addr, int socklen, void *arg) {
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
    
    printf("New client connected\n");
}

// Read callback
static void read_cb(struct bufferevent *bev, void *arg) {
    struct evbuffer *input = bufferevent_get_input(bev);
    size_t len = evbuffer_get_length(input);
    char *request = malloc(len + 1);
    
    if (request) {
        evbuffer_copyout(input, request, len);
        request[len] = '\0';
        
        printf("\nReceived RTSP request:\n%s\n", request);
        
        char *response = NULL;
        if (handle_rtsp_request(request, &response) == 0 && response) {
            printf("\nSending RTSP response:\n%s\n", response);
            bufferevent_write(bev, response, strlen(response));
            free(response);
        }
        free(request);
        
        // Clear the input buffer
        evbuffer_drain(input, len);
    }
}

// Event callback
static void event_cb(struct bufferevent *bev, short events, void *arg) {
    if (events & BEV_EVENT_ERROR) {
        fprintf(stderr, "Error from bufferevent\n");
    }
    if (events & (BEV_EVENT_EOF | BEV_EVENT_ERROR)) {
        printf("Client disconnected\n");
        bufferevent_free(bev);
    }
}

// Handle RTSP request
static int handle_rtsp_request(const char *request, char **response) {
    printf("Processing RTSP request...\n");
    
    if (strncmp(request, "POST /pair-setup", 16) == 0) {
        printf("Handling pair-setup request\n");
        return auth_handle_pair_setup(request, response);
    }
    else if (strncmp(request, "POST /pair-verify", 17) == 0) {
        printf("Handling pair-verify request\n");
        return auth_handle_pair_verify(request, response);
    }
    else if (strncmp(request, "POST /fp-setup", 13) == 0) {
        printf("Handling FairPlay setup request\n");
        
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
    else if (strncmp(request, "FPLY", 4) == 0) {
        printf("Handling FairPlay request\n");
        // FairPlay response
        *response = strdup("RTSP/1.0 200 OK\r\n"
                          "Content-Type: application/octet-stream\r\n"
                          "Content-Length: 0\r\n"
                          "\r\n");
        return 0;
    }
    else if (strncmp(request, "OPTIONS", 7) == 0) {
        printf("Handling OPTIONS request\n");
        *response = strdup("RTSP/1.0 200 OK\r\n"
                          "Public: ANNOUNCE, SETUP, RECORD, PAUSE, FLUSH, TEARDOWN, OPTIONS, GET_PARAMETER, SET_PARAMETER\r\n"
                          "\r\n");
        return 0;
    }
    else if (strncmp(request, "ANNOUNCE", 8) == 0) {
        printf("Handling ANNOUNCE request\n");
        *response = strdup("RTSP/1.0 200 OK\r\n\r\n");
        return 0;
    }
    else if (strncmp(request, "SETUP", 5) == 0) {
        printf("Handling SETUP request\n");
        *response = strdup("RTSP/1.0 200 OK\r\n"
                          "Session: 1\r\n"
                          "Transport: RTP/AVP/UDP;unicast;client_port=5000-5001;server_port=5002-5003\r\n"
                          "\r\n");
        return 0;
    }
    else if (strncmp(request, "RECORD", 6) == 0) {
        printf("Handling RECORD request\n");
        *response = strdup("RTSP/1.0 200 OK\r\n"
                          "Session: 1\r\n"
                          "Range: npt=0.000-\r\n"
                          "\r\n");
        return 0;
    }
    else if (strncmp(request, "GET_PARAMETER", 13) == 0) {
        printf("Handling GET_PARAMETER request\n");
        *response = strdup("RTSP/1.0 200 OK\r\n"
                          "Session: 1\r\n"
                          "Content-Type: text/parameters\r\n"
                          "Content-Length: 0\r\n"
                          "\r\n");
        return 0;
    }
    else if (strncmp(request, "SET_PARAMETER", 13) == 0) {
        printf("Handling SET_PARAMETER request\n");
        *response = strdup("RTSP/1.0 200 OK\r\n"
                          "Session: 1\r\n"
                          "\r\n");
        return 0;
    }

    printf("Unhandled request type\n");
    // Default response for unhandled requests
    *response = strdup("RTSP/1.0 501 Not Implemented\r\n\r\n");
    return 0;
}

// Generate FairPlay key pair
static int generate_fp_key_pair(unsigned char **public_key, size_t *public_key_len) {
    EVP_PKEY_CTX *ctx = NULL;
    EVP_PKEY *pkey = NULL;
    OSSL_PARAM params[2];
    size_t bits = 2048;
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

    // Set key size to 2048 bits
    params[0] = OSSL_PARAM_construct_size_t(OSSL_PKEY_PARAM_RSA_BITS, &bits);
    params[1] = OSSL_PARAM_construct_end();
    
    if (EVP_PKEY_CTX_set_params(ctx, params) <= 0) {
        printf("Failed to set key parameters\n");
        goto cleanup;
    }

    // Generate key pair
    if (EVP_PKEY_generate(ctx, &pkey) <= 0) {
        printf("Failed to generate key pair\n");
        goto cleanup;
    }

    // Get public key in DER format
    size_t len = 0;
    if (EVP_PKEY_get_octet_string_param(pkey, OSSL_PKEY_PARAM_PUB_KEY, NULL, 0, &len) <= 0) {
        printf("Failed to get public key length\n");
        goto cleanup;
    }

    *public_key = malloc(len);
    if (!*public_key) {
        printf("Failed to allocate memory for public key\n");
        goto cleanup;
    }

    if (EVP_PKEY_get_octet_string_param(pkey, OSSL_PKEY_PARAM_PUB_KEY, *public_key, len, &len) <= 0) {
        printf("Failed to get public key\n");
        free(*public_key);
        *public_key = NULL;
        goto cleanup;
    }

    *public_key_len = len;
    ret = 0;

cleanup:
    EVP_PKEY_free(pkey);
    EVP_PKEY_CTX_free(ctx);
    return ret;
} 