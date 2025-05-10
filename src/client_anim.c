#include "net.h"
#include <stdio.h>
#include <unistd.h>

#define SERVER_IP "127.0.0.1"
#define PORT 5000

int main() {
    int sock = connect_to_server(SERVER_IP, PORT);
    if (sock < 0) {
        perror("No se pudo conectar al servidor");
        return 1;
    }

    printf("🕓 Esperando canvas del servidor...\n");

    while (1) {
        NetPacket pkt;
        if (recv_packet(sock, &pkt) <= 0) {
            perror("recv_packet");
            printf("❌ Error recibiendo paquete (¿servidor cerró?)\n");
            break;
        }
    
        // Limpiar pantalla antes de mostrar
        printf("\033[H\033[J"); // ANSI clear screen
    
        printf("📺 Canvas recibido (%dx%d) en posición (%d, %d):\n", pkt.width, pkt.height, pkt.x, pkt.y);
        for (int i = 0; i < pkt.height; i++) {
            for (int j = 0; j < pkt.width; j++) {
                putchar(pkt.data[i][j]);
            }
            putchar('\n');
        }
    
        usleep(50000); // ~20 FPS
    }
    

    close(sock);
    return 0;
}
