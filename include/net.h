#ifndef NET_H
#define NET_H

// Representa un paquete de datos con una sección del canvas para enviar por red.
// Entrada: posición y dimensiones del contenido a transmitir.
// Salida: datos listos para enviar o recibir mediante sockets.
typedef struct {
    int x, y;
    int width, height;
    char data[100][200];  // max height x width
} NetPacket;

// Crea un socket de servidor y lo deja a la escucha en el puerto especificado.
// Entrada: número de puerto donde escuchar conexiones entrantes.
// Salida: descriptor del socket del servidor, o -1 si hubo error.
int create_server_socket(int port);

// Acepta una conexión entrante en el socket del servidor.
// Entrada: descriptor del socket del servidor.
// Salida: descriptor del nuevo socket cliente, o -1 si hubo error.
int accept_client(int server_fd);

// Se conecta a un servidor usando su IP y puerto.
// Entrada: dirección IP del servidor y número de puerto.
// Salida: descriptor del socket conectado, o -1 si falló.
int connect_to_server(const char* ip, int port);

// Envía un paquete de datos a través de un socket.
// Entrada: descriptor del socket y puntero al paquete NetPacket a enviar.
// Salida: número de bytes enviados, o -1 si ocurrió un error.
int send_packet(int sockfd, NetPacket* pkt);

// Recibe un paquete de datos desde un socket.
// Entrada: descriptor del socket y puntero al paquete NetPacket donde guardar los datos.
// Salida: número de bytes recibidos, o -1 si falló la recepción.
int recv_packet(int sockfd, NetPacket* pkt);

#endif
