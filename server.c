
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
#define BUF_SIZE 256

typedef enum client_pack_type_e client_pack_type_e;
typedef enum server_pack_type_e server_pack_type_e;

typedef struct request_s request_s;

enum client_pack_type_e {
    PACKET_INIT,
    PACKET_TX,
    PACKET_REESTAB
};

enum server_pack_type_e
{
    PACKET_SESSID,
};

struct request_s {
    int fd;
    bool isactive;
    struct sockaddr_in client_ip;
    char ipstr[INET_ADDRSTRLEN];
    pthread_t thread;
    uint64_t session_id;
};

#define PASSWORD "test"

static time_t base_time;
static uint64_t session_counter;

static bool isrunning;
static void *serve_client(void *arg);
static request_s *request_s_(int fd, struct sockaddr_in *client_ip);
static bool check_request(request_s *req);
static bool authenticate(request_s *req);
static bool pass_correct(char *pass);
static uint64_t new_session_id(void);
static void tx_new_session_id(request_s *req);
static void resolve_remote(request_s *req);

void server_start(uint16_t port)
{
    struct sockaddr_in sock_addr;
    
    base_time = time(NULL);
    
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
    request_s *req = arg;
    struct pollfd fdstr;
    ssize_t status;
    
    if(authenticate(req)) {
        log_info("Client [%s] Successfully Authenticated.", req->ipstr);
    }
    else {
        req->session_id = session_counter++;
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

bool check_request(request_s *req)
{
    ssize_t status;
    char buf[BUF_SIZE], pass[BUF_SIZE];
    static const char fail[] = "/-------------FAIL!!!--------------/\a\n";
    
    status = read(req->fd, buf, BUF_SIZE);
    if(status < 0) {
        log_error("Failure during read during validation of new connection. Socket: %d, Client: %s.", req->fd, req->ipstr);
        goto exit;
    }
    
    switch(buf[0]) {
        case PACKET_INIT:
            if(pass_correct(&buf[1])) {
                tx_new_session_id(req);
            }
            break;
        case PACKET_REESTAB:
            
            break;
        default:
            //fail
            break;
    }
    
exit:
    return false;
}

bool pass_correct(char *pass)
{
    int i;
    const char *pptr = pass;
    
    for(i = 0; i < BUF_SIZE; i++) {
        if(*pptr) {
            if(*pass == *pptr) {
                pass++;
                pptr++;
            }
        }
        else {
            return false;
        }
    }
    if(!(*pass || *pptr))
        return true;
    else
        return false;
}

void tx_new_session_id(request_s *req)
{
    uint64_t sid = new_session_id();
    ssize_t status;
    char buf[9];
 
    req->session_id = sid;
    
    buf[0] = PACKET_SESSID;
    memcpy(&buf[1], &sid, sizeof sid);
    
    status = write(req->fd, buf, sizeof buf);
    if(status < 0) {
        log_error("Failed to send new session id");
    }
}

uint64_t new_session_id(void)
{
    static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
    
    uint64_t newid;
    
    pthread_mutex_lock(&lock);
    newid = session_counter++;
    pthread_mutex_unlock(&lock);
    
    return newid;
}


bool authenticate(request_s *req)
{
    enum {NFAILS = 50000};
    static const char message[] =
    "/----------------------------------/\n"
    "/--------AUTHENTICATE BITCH--------/\n"
    "/----------------------------------/\a\n>"
    ;
    
    static const char fail[] = "/-------------FAIL!!!--------------/\a\n";
    
    static const char success[] = "success.";
    int i;
    char buf[BUF_SIZE] = {0};
    ssize_t status;
    
    status = read(req->fd, buf, sizeof(buf));
    if(status < 0) {
        log_error("Failed to read on socket %d from client [%s]", req->fd, req->ipstr);
        return false;
    }
    
    if(buf[0] != PACKET_INIT) {
        int count = 0;
        while(buf[0] != PACKET_INIT) {
            status = write(req->fd, message, sizeof(message));
            if(status < 0) {
                log_error("Write failed on socket: %d.", req->fd);
                return false;
            }
            status = read(req->fd, buf, BUF_SIZE);
            if(status < 0) {
                log_error("Read failed on socket: %d.", req->fd);
                return false;
            }
            if(count == 3) {
                return false;
            }
            else {
                count++;
            }
        }
    }
    else {
        status = read(req->fd, buf, BUF_SIZE);
        if(status < 0) {
            log_error("Read failed on socket: %d.", req->fd);
        }
    }
    
    buf[status - 2] = '\0';
    
    if(strcmp(PASSWORD, buf)) {
        log_warn("Failed Authentication Attempt Ocurred from client: [%s].", req->ipstr);
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

void resolve_remote(request_s *req)
{
    ssize_t status;
    char buf[BUF_SIZE], *bptr;
    
    status = read(req->fd, buf, BUF_SIZE);
    if(status < 0) {
        log_error("Failed to read socket %d while attempting to resolve remote server.", req->fd);
    }
    
    while(*bptr && bptr - buf < 256) {
        
        bptr++;
    }
}

