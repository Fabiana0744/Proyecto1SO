#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <ncurses.h>

#include "../include/anim.h"

#define SERVER_IP "127.0.0.1"
#define SERVER_PORT 8080

int main() {
    int sock;
    struct sockaddr_in server_addr;
    NetPacket pkt;

    // Crear socket
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("socket");
        return 1;
    }

    // Datos del servidor
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);
    inet_pton(AF_INET, SERVER_IP, &server_addr.sin_addr);

    // Conectar
    if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("connect");
        return 1;
    }

    // Inicializar ncurses
    initscr(); noecho(); curs_set(0);
    timeout(0);  // lectura no bloqueante de teclado

    while (1) {
        ssize_t n = read(sock, &pkt, sizeof(NetPacket));
        if (n <= 0) break;  // conexión cerrada

        clear();
        for (int y = 0; y < pkt.height; y++) {
            for (int x = 0; x < pkt.width; x++) {
                char ch = pkt.content[y * pkt.width + x];
                mvaddch(y + 1, x + 1, ch);
            }
        }
        refresh();

        // Salida manual (opcional): presiona 'q'
        int ch = getch();
        if (ch == 'q' || ch == 'Q') break;

        usleep(10000);  // evitar sobrecarga de CPU
    }

    endwin();
    close(sock);
    return 0;
}
