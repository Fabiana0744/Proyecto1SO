#ifndef NET_H
#define NET_H

typedef struct {
    int x, y;
    int width, height;
    char data[100][200];  // max height x width
} NetPacket;

int create_server_socket(int port);
int accept_client(int server_fd);
int connect_to_server(const char* ip, int port);
int send_packet(int sockfd, NetPacket* pkt);
int recv_packet(int sockfd, NetPacket* pkt);

#endif
