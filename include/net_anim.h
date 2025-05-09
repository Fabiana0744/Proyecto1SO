// include/net_anim.h
#ifndef NET_ANIM_H
#define NET_ANIM_H

#include <stddef.h>  // Añadir esto para size_t
#include <stdint.h>

#define PORT 8080
#define MAX_CLIENTS 10
#define BUFFER_SIZE 1024

typedef struct {
    int object_id;
    int x;
    int y;
    int rotation;
    long timestamp;
} NetPacket;

// Declarar todas las funciones
int init_server();
int connect_to_server(const char *ip);
void send_to_all(const void *data, size_t len);
void net_cleanup();

#endif