#include "net.h"
#include <stdio.h>
#include <unistd.h>
#include "mypthreads.h"
#include <signal.h>

#define SERVER_IP "127.0.0.1"
#define PORT 5000

static volatile sig_atomic_t running = 1;

void run_client(const char* host, int port)
{

    int sock = connect_to_server(host, port);
    if (sock < 0) {
        perror("No se pudo conectar al servidor");
        exit(EXIT_FAILURE);
    }

    printf("📡 Conectado a %s:%d – esperando canvas…\n", host, port);

    while (running) {
        NetPacket pkt;
        int n = recv_packet(sock, &pkt);
        if (n <= 0) {
            if (running)   /* si no fue Ctrl-C */
                perror("recv_packet");
            break;
        }

        /* clear-screen ANSI */
        printf("\033[H\033[J");
        fflush(stdout);

        /* mostrar franja */
        for (int r = 0; r < pkt.height; ++r) {
            fwrite(pkt.data[r], 1, pkt.width, stdout);
            putchar('\n');
        }

        busy_wait_ms(50);          /* limitador de FPS */
    }

    close(sock);
    printf("🔌 Cliente cerrado.\n");
}