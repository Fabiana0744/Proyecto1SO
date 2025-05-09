#include "../include/anim.h"
#include "../include/mypthreads.h"
#include "../include/net_anim.h"
#include <pthread.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

my_mutex_t draw_mutex;

int client_sockets[MAX_CLIENTS] = {0};
pthread_mutex_t net_mutex = PTHREAD_MUTEX_INITIALIZER;

// --- Funciones de red ---
void send_to_all(const void *data, size_t len) {
    pthread_mutex_lock(&net_mutex);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (client_sockets[i] > 0) {
            send(client_sockets[i], data, len, 0);
        }
    }
    pthread_mutex_unlock(&net_mutex);
}

int init_server() {
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("socket");
        return -1;
    }

    struct sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
        perror("bind");
        return -1;
    }

    if (listen(server_fd, MAX_CLIENTS) < 0) {
        perror("listen");
        return -1;
    }

    printf("✅ Servidor escuchando en puerto %d\n", PORT);
    return server_fd;
}

void* client_handler(void *arg) {
    int sock = *(int*)arg;
    NetPacket pkt;
    while (1) {
        int bytes = recv(sock, &pkt, sizeof(pkt), 0);
        if (bytes <= 0) {
            printf("❌ Cliente %d desconectado\n", sock);
            break;
        }
        printf("📥 Recibido de %d: ID=%d\n", sock, pkt.object_id);
    }
    close(sock);
    return NULL;
}

void* network_main(void *arg) {
    int server_fd = *(int*)arg;
    struct sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);
    
    while (1) {
        int client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &addr_len);
        if (client_fd < 0) {
            perror("accept");
            continue;
        }
        printf("🔄 Cliente conectado desde %s (Socket %d)\n", inet_ntoa(client_addr.sin_addr), client_fd);
        
        pthread_mutex_lock(&net_mutex);
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (client_sockets[i] == 0) {
                client_sockets[i] = client_fd;
                pthread_t thread;
                pthread_create(&thread, NULL, client_handler, &client_sockets[i]);
                break;
            }
        }
        pthread_mutex_unlock(&net_mutex);
    }
    return NULL;
}

// --- Lógica de animación ---
void distributed_render(int id, int x, int y, int rot) {
    NetPacket pkt = {
        .object_id = id,
        .x = x,
        .y = y,
        .rotation = rot,
        .timestamp = get_current_time_ms()
    };
    send_to_all(&pkt, sizeof(pkt));
    printf("📤 Enviando: ID=%d, X=%d, Y=%d\n", id, x, y);
}

int main() {
    my_thread_register_main();
    
    // Iniciar servidor de red
    int server_fd = init_server();
    if (server_fd < 0) return 1;

    pthread_t net_thread;
    pthread_create(&net_thread, NULL, network_main, (void*)&server_fd);

    int pos_x = 0;
    int max_x = 100; // Define un límite alto o usa INT_MAX para teóricamente infinito

    sleep(7);

    while (1) {
        distributed_render(1, pos_x, 5, 0);
        pos_x = (pos_x + 1) % max_x; // Opcional: reinicia después de 1000
        usleep(200000);
    }

    return 0;
}