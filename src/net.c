#include "net.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>

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

int accept_client(int server_fd) {
    struct sockaddr_in addr;
    socklen_t len = sizeof(addr);
    return accept(server_fd, (struct sockaddr*)&addr, &len);
}

int connect_to_server(const char* ip, int port) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    inet_pton(AF_INET, ip, &addr.sin_addr);
    connect(sock, (struct sockaddr*)&addr, sizeof(addr));
    return sock;
}

int send_packet(int sockfd, NetPacket* pkt) {
    return send(sockfd, pkt, sizeof(NetPacket), 0);
}

int recv_packet(int sockfd, NetPacket* pkt) {
    return recv(sockfd, pkt, sizeof(NetPacket), 0);
}
