#include "auth.h"
#include "auth_keys.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <openssl/rand.h>
#include <openssl/sha.h>
#include <openssl/hmac.h>
#include <openssl/evp.h>
#include <openssl/bio.h>
#include <openssl/buffer.h>
#include <openssl/pem.h>
#include <json-c/json.h>

#define SALT_LENGTH 16
#define PIN_LENGTH 8
#define KEY_LENGTH 32

static EVP_PKEY *private_key = NULL;
static unsigned char salt[SALT_LENGTH];
static char pin[PIN_LENGTH + 1];
static unsigned char session_key[KEY_LENGTH];

int auth_init(void) {
    // Load private key from PEM string
    BIO *bio = BIO_new_mem_buf(AIRPLAY_PRIVATE_KEY, -1);
    if (!bio) {
        fprintf(stderr, "Failed to create BIO for private key\n");
        return -1;
    }

    private_key = PEM_read_bio_PrivateKey(bio, NULL, NULL, NULL);
    BIO_free(bio);

    if (!private_key) {
        fprintf(stderr, "Failed to load private key\n");
        return -1;
    }

    return 0;
}

void auth_cleanup(void) {
    if (private_key) {
        EVP_PKEY_free(private_key);
        private_key = NULL;
    }
}

EVP_PKEY *auth_get_private_key(void) {
    return private_key;
}

static void generate_salt(void) {
    if (RAND_bytes(salt, SALT_LENGTH) != 1) {
        fprintf(stderr, "Failed to generate random salt\n");
        return;
    }
}

static void generate_pin(void) {
    // Generate a random 8-digit PIN
    for (int i = 0; i < PIN_LENGTH; i++) {
        pin[i] = '0' + (rand() % 10);
    }
    pin[PIN_LENGTH] = '\0';
}

int auth_handle_pair_setup(const char *request, char **response) {
    // Generate new salt and PIN for this pairing attempt
    generate_salt();
    generate_pin();

    // Create response with salt
    char *salt_hex = malloc(SALT_LENGTH * 2 + 1);
    for (int i = 0; i < SALT_LENGTH; i++) {
        sprintf(salt_hex + (i * 2), "%02x", salt[i]);
    }
    salt_hex[SALT_LENGTH * 2] = '\0';

    // Format response
    char *resp = malloc(1024);
    snprintf(resp, 1024,
        "RTSP/1.0 200 OK\r\n"
        "Content-Type: application/octet-stream\r\n"
        "Content-Length: %d\r\n"
        "\r\n"
        "{\"salt\":\"%s\",\"pin\":\"%s\"}",
        (int)(strlen(salt_hex) + strlen(pin) + 20),
        salt_hex,
        pin);

    *response = resp;
    free(salt_hex);
    return 0;
}

static char *base64_encode(const unsigned char *input, int length) {
    BIO *bmem, *b64;
    BUF_MEM *bptr;
    char *buff;

    b64 = BIO_new(BIO_f_base64());
    bmem = BIO_new(BIO_s_mem());
    b64 = BIO_push(b64, bmem);
    BIO_write(b64, input, length);
    BIO_flush(b64);
    BIO_get_mem_ptr(b64, &bptr);

    buff = (char *)malloc(bptr->length);
    memcpy(buff, bptr->data, bptr->length-1);
    buff[bptr->length-1] = 0;

    BIO_free_all(b64);

    return buff;
}

static unsigned char *base64_decode(const char *input, int *length) {
    BIO *b64, *bmem;
    unsigned char *buffer;
    size_t len = strlen(input);

    buffer = (unsigned char *)malloc(len);
    memset(buffer, 0, len);

    b64 = BIO_new(BIO_f_base64());
    bmem = BIO_new_mem_buf(input, len);
    bmem = BIO_push(b64, bmem);

    *length = BIO_read(bmem, buffer, len);
    BIO_free_all(bmem);

    return buffer;
}

int auth_handle_pair_verify(const char *request, char **response) {
    struct json_object *json;
    struct json_object *public_key_obj, *signature_obj;
    const char *client_public_key;
    const char *client_signature;
    int client_key_len, sig_len;
    unsigned char *decoded_key, *decoded_sig;
    
    // Parse the request body
    const char *body = strstr(request, "\r\n\r\n");
    if (!body) {
        *response = strdup("RTSP/1.0 400 Bad Request\r\n\r\n");
        return -1;
    }
    body += 4;

    json = json_tokener_parse(body);
    if (!json) {
        *response = strdup("RTSP/1.0 400 Bad Request\r\n\r\n");
        return -1;
    }

    // Get client's public key and signature
    if (!json_object_object_get_ex(json, "publicKey", &public_key_obj) ||
        !json_object_object_get_ex(json, "signature", &signature_obj)) {
        json_object_put(json);
        *response = strdup("RTSP/1.0 400 Bad Request\r\n\r\n");
        return -1;
    }

    client_public_key = json_object_get_string(public_key_obj);
    client_signature = json_object_get_string(signature_obj);

    // Decode base64 values
    decoded_key = base64_decode(client_public_key, &client_key_len);
    decoded_sig = base64_decode(client_signature, &sig_len);

    // Generate server's public key
    unsigned char server_public_key[KEY_LENGTH];
    if (RAND_bytes(server_public_key, KEY_LENGTH) != 1) {
        free(decoded_key);
        free(decoded_sig);
        json_object_put(json);
        *response = strdup("RTSP/1.0 500 Internal Server Error\r\n\r\n");
        return -1;
    }

    // Generate session key
    if (RAND_bytes(session_key, KEY_LENGTH) != 1) {
        free(decoded_key);
        free(decoded_sig);
        json_object_put(json);
        *response = strdup("RTSP/1.0 500 Internal Server Error\r\n\r\n");
        return -1;
    }

    // Create response
    char *server_key_b64 = base64_encode(server_public_key, KEY_LENGTH);
    char *session_key_b64 = base64_encode(session_key, KEY_LENGTH);

    char *resp = malloc(1024);
    snprintf(resp, 1024,
        "RTSP/1.0 200 OK\r\n"
        "Content-Type: application/octet-stream\r\n"
        "Content-Length: %d\r\n"
        "\r\n"
        "{\"publicKey\":\"%s\",\"sessionKey\":\"%s\"}",
        (int)(strlen(server_key_b64) + strlen(session_key_b64) + 40),
        server_key_b64,
        session_key_b64);

    // Cleanup
    free(decoded_key);
    free(decoded_sig);
    free(server_key_b64);
    free(session_key_b64);
    json_object_put(json);

    *response = resp;
    return 0;
} 