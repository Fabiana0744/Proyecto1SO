#include "net.h"
#include <stdio.h>
#include <unistd.h>
#include "mypthreads.h"
#include <signal.h>

#define SERVER_IP "127.0.0.1"  // Dirección IP por defecto del servidor.
#define PORT 5000              // Puerto por defecto del servidor.

// Variable de control para terminar el bucle principal cuando se reciba una señal.
// Entrada: modificada por señal externa.
// Salida: indica si el cliente debe seguir ejecutándose.
static volatile sig_atomic_t running = 1;

// Ejecuta el cliente que se conecta a un servidor y muestra las actualizaciones del canvas.
// Entrada: dirección IP del servidor y puerto de conexión.
// Salida: representación del canvas en la terminal hasta que se interrumpa.
void run_client(const char* host, int port)
{
    // Intenta conectarse al servidor.
    // Entrada: IP y puerto del servidor.
    // Salida: descriptor del socket o termina el programa si falla.
    int sock = connect_to_server(host, port);
    if (sock < 0) {
        perror("No se pudo conectar al servidor");
        exit(EXIT_FAILURE);
    }

    printf("📡 Conectado a %s:%d – esperando canvas…\n", host, port);

    // Bucle principal que recibe y muestra secciones del canvas.
    while (running) {
        NetPacket pkt;

        // Recibe un paquete desde el socket.
        // Entrada: socket del servidor.
        // Salida: estructura NetPacket con datos visuales.
        int n = recv_packet(sock, &pkt);
        if (n <= 0) {
            if (running)   // Si no fue por señal Ctrl-C
                perror("recv_packet");
            break;
        }

        // Limpia la pantalla con secuencia.
        printf("\033[H\033[J");
        fflush(stdout);

        // Muestra la franja recibida línea por línea.
        for (int r = 0; r < pkt.height; ++r) {
            fwrite(pkt.data[r], 1, pkt.width, stdout);
            putchar('\n');
        }

        // Espera activa para limitar la frecuencia de actualización.
        busy_wait_ms(50);
    }

    // Cierra el socket al finalizar la ejecución del cliente.
    close(sock);
    printf("🔌 Cliente cerrado.\n");
}
