
#include "server.h"
#include "general.h"
#include "log.h"

#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

void server_start(uint16_t port)
{
    struct sockaddr_in sock_addr;
    
    log_debug("Starting Server on port: %d", port);
    
    int sock_fd = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);

    if(sock_fd == -1) {
        perror("Error Creating socket");
        exit(EXIT_FAILURE);
    }
    
    
    memset(&sock_addr, 0, sizeof(sock_addr));
    sock_addr.sin_family = AF_INET;
    sock_addr.sin_port = htons(port);
    sock_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    
    int status = bind(sock_fd, (struct sockaddr *)&sock_addr, sizeof(sock_addr));
    if(status == -1) {
        perror("Error biding socket");
        exit(EXIT_FAILURE);
    }
        
}
