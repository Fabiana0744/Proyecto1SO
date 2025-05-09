#include "../include/anim.h"
#include "../include/net_anim.h"
#include <ncurses.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

Canvas local_canvas;

int connect_to_server(const char *ip) {
    printf("🔗 Conectando a %s...\n", ip);
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("socket");
        return -1;
    }

    struct sockaddr_in serv_addr;
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);

    if (inet_pton(AF_INET, ip, &serv_addr.sin_addr) <= 0) {
        perror("inet_pton");
        close(sock);
        return -1;
    }

    if (connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr))) {
        perror("connect");
        close(sock);
        return -1;
    }
    
    printf("✅ Conexión exitosa al servidor\n");
    return sock;
}

void update_local_display(NetPacket *pkt) {
    int canvas_width = 100; // Ancho del canvas desde animation.ini
    int display_x = pkt->x % canvas_width; // Wrap alrededor del canvas
    
    mvprintw(pkt->y, display_x, "X"); 
    refresh();
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Uso: %s <IP-servidor>\n", argv[0]);
        return 1;
    }
    
    int sock = connect_to_server(argv[1]);
    if (sock < 0) return 1;

    initscr(); 
    noecho(); 
    curs_set(0);
    
    while (1) {
        NetPacket pkt;
        int bytes = recv(sock, &pkt, sizeof(pkt), 0);
        if (bytes <= 0) {
            printf("🚨 Error de conexión\n");
            break;
        }
        update_local_display(&pkt);
    }
    
    endwin();
    close(sock);
    return 0;
}