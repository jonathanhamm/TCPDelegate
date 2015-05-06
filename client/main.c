#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define DEFAULT_PORT 13370
#define DEFAULT_SERVER "127.0.0.1"
#define PASS "test"

enum client_pack_type_e {
    PACKET_INIT = 1,
    PACKET_TX,
    PACKET_REESTAB
};

static void client_connect(const char *server, uint16_t port);
static void init_write(int fd, const char *pass);


int main(int argc, const char *argv[]) {
    client_connect(DEFAULT_SERVER, DEFAULT_PORT);
    return 0;
}

void client_connect(const char *server, uint16_t port)
{
    int fd, status;
    struct sockaddr_in serv_addr;
    
    fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if(fd < 0) {
        perror("Error: Failed to create socket");
        exit(EXIT_FAILURE);
    }
    
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = inet_addr(server);
    serv_addr.sin_port = htons(DEFAULT_PORT);
    
    status = connect(fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
    if(connect < 0) {
        perror("Connection Error");
        exit(EXIT_FAILURE);
    }
    
    init_write(fd, PASS);
    
    close(fd);
}

void init_write(int fd, const char *pass)
{
    char buf[128];
    
    buf[0] = PACKET_INIT;
    strcpy(&buf[1], pass);
    
    write(fd, buf, strlen(buf) + 1);
}
