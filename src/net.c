#include "net.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>

// Crea un socket de servidor y lo configura para escuchar conexiones entrantes.
// Entrada: número de puerto en el que el servidor escuchará.
// Salida: descriptor del socket del servidor, o -1 si falla algún paso.
int create_server_socket(int port) {
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = INADDR_ANY;

    bind(server_fd, (struct sockaddr*)&addr, sizeof(addr));
    listen(server_fd, 5);
    return server_fd;
}

// Acepta una conexión entrante desde un cliente.
// Entrada: descriptor del socket del servidor.
// Salida: descriptor del nuevo socket conectado al cliente, o -1 si falla.
int accept_client(int server_fd) {
    struct sockaddr_in addr;
    socklen_t len = sizeof(addr);
    return accept(server_fd, (struct sockaddr*)&addr, &len);
}

// Se conecta a un servidor remoto usando IP y puerto especificados.
// Entrada: dirección IP del servidor y número de puerto.
// Salida: descriptor del socket conectado, o -1 si falla la conexión.
int connect_to_server(const char* ip, int port) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    inet_pton(AF_INET, ip, &addr.sin_addr);
    connect(sock, (struct sockaddr*)&addr, sizeof(addr));
    return sock;
}

// Envía un paquete NetPacket a través de un socket.
// Entrada: descriptor del socket y puntero al paquete NetPacket a enviar.
// Salida: número de bytes enviados, o -1 si ocurre un error.
int send_packet(int sockfd, NetPacket* pkt) {
    return send(sockfd, pkt, sizeof(NetPacket), 0);
}

// Recibe un paquete NetPacket desde un socket.
// Entrada: descriptor del socket y puntero donde almacenar el paquete recibido.
// Salida: número de bytes recibidos, o -1 si ocurre un error.
int recv_packet(int sockfd, NetPacket* pkt) {
    return recv(sockfd, pkt, sizeof(NetPacket), 0);
}
