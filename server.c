
#include "server.h"
#include "general.h"
#include "log.h"

#include <string.h>
#include <unistd.h>
#include <poll.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define MAX_TIMEOUT 10000

typedef enum pack_type_e pack_type_e;

typedef struct request_s request_s;

enum pack_type_e {
    PACKET_OPEN = 1,
    PACKET_CLOSE,
    PACKET_TX_SERVER,
    PACKET_TX_CLIENT,
    PACKET_SQL_QUERY,
    PACKET_TARGETINFO};

struct request_s {
    int fd;
    bool isactive;
    struct sockaddr_in client_ip;
    char ipstr[INET_ADDRSTRLEN];
    pthread_t thread;
};


#define PASSWORD "test"

static bool isrunning;
static void *serve_client(void *arg);
static request_s *request_s_(int fd, struct sockaddr_in *client_ip);
static bool authenticate(request_s *req);

void server_start(uint16_t port)
{
    struct sockaddr_in sock_addr;
    
    log_info("Starting Server on port: %d.", port);
    
    signal(SIGPIPE, SIG_IGN);
    
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
        perror("Error biding socket. Call to bind() failed.");
        exit(EXIT_FAILURE);
    }
    
    status = listen(sock_fd, MAX_CLIENTS);
    if(status == -1) {
        perror("call to listen() failed");
        close(sock_fd);
    }
    
    isrunning = true;
    
    log_info("Server is now listening on port: %d.", port);

    while(isrunning) {
        request_s *req;
        struct sockaddr_in client_ip;
        socklen_t len = sizeof(client_ip);
        int client_fd = accept(sock_fd, (struct sockaddr *)&client_ip, &len);
        
        req = request_s_(client_fd, &client_ip);

        if(client_fd < 0) {
            perror("Client failed on Connection Attempt");
            log_error("Client [%s] experienced connection error.", req->ipstr);
            free(req);
        }
        else {
            log_info("Client [%s] Connected with socket descriptor: %d.", req->ipstr, client_fd);
            

            status = pthread_create(&req->thread, NULL, serve_client, req);
            if(status < 0) {
                log_error("Failure to create thread for client [%s].", req->ipstr);
            }
        }
    }
    close(sock_fd);
}

void *serve_client(void *arg)
{
    enum {BUF_SIZE = 256};
    request_s *req = arg;
    struct pollfd fdstr;
    
    
    if(authenticate(req)) {
        log_info("Client [%s] Successfully Authenticated.", req->ipstr);
    }
    else {
        goto exit;
    }
    
    fdstr.fd = req->fd;
    fdstr.events = POLLIN | POLLRDNORM | POLLRDBAND | POLLPRI | POLLOUT | POLLWRBAND | POLLERR | POLLHUP | POLLNVAL;
    
    while(req->isactive) {
        int status = poll(&fdstr, 1, MAX_TIMEOUT);
        if(status == 1) {
            switch(fdstr.revents) {
                case POLLIN | POLLRDNORM | POLLRDBAND | POLLPRI:
                    
                    break;
                case POLLOUT | POLLWRBAND:
                    break;
                case POLLHUP:
                    break;
                case POLLNVAL:
                    break;
            }
        }
        else if(status == 0) {
            log_error("Attempt to read on socket %d timed out.", fdstr.fd);
            goto exit;
        }
        else {
            log_error("An error occured while trying to read on socket %d. Errno: %d.", fdstr.fd, errno);
        }
    }

exit:
    close(req->fd);
    free(req);
    pthread_exit(NULL);
}

request_s *request_s_(int fd, struct sockaddr_in *client_ip)
{
    int ip = client_ip->sin_addr.s_addr;
    request_s *req = del_alloc(sizeof *req);
    req->fd = fd;
    req->isactive = true;
    req->client_ip = *client_ip;
    inet_ntop(AF_INET, &ip, req->ipstr, INET_ADDRSTRLEN);
    return req;
}

bool authenticate(request_s *req)
{
    enum {BUF_SIZE = 256, NFAILS = 50000};
    static const char message[] =
    "/----------------------------------/\n"
    "/--------AUTHENTICATE BITCH--------/\n"
    "/----------------------------------/\a\n>"
    ;
    
    static const char fail[] = "/-------------FAIL!!!--------------/\a\n";
    
    static const char success[] = "success.";
    int i;
    char buf[BUF_SIZE];
    ssize_t status;
    
    status = write(req->fd, message, sizeof(message));
    if(status < 0) {
        log_error("Write failed on socket: %d.", req->fd);
    }
    
    status = read(req->fd, buf, BUF_SIZE);
    if(status < 0) {
        log_error("Read failed on socket: %d.", req->fd);
    }
    
    buf[status - 2] = '\0';
    
    if(strcmp(PASSWORD, buf)) {
        log_warn("Failed Authentication Attempt Ocurred from client: [%s]", req->ipstr);
        for(i = 0; i < NFAILS; i++) {
            status = write(req->fd, fail, sizeof(fail));
            if(status < 0) {
                log_warn("Failed to send authentication failure message - Probably Broken Pipe.");
                break;
            }
        }
        return false;
    }
    else {
        log_info("Successful Login Occurred");
        status = write(req->fd, success, sizeof(success));
        if(status < 0) {
            log_error("Failed to send authentication success message.");
        }
        return true;
    }
}



