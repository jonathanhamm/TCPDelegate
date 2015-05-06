
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
#define TABLE_SIZE 19

typedef enum client_pack_type_e client_pack_type_e;

typedef struct request_s request_s;

enum client_pack_type_e {
    PACKET_INIT = 1,
    PACKET_TX,
    PACKET_REESTAB,
    PACKET_SESSIONID
};

struct request_s {
    int fd;
    bool isactive;
    struct sockaddr_in client_ip;
    char ipstr[INET_ADDRSTRLEN];
    pthread_t thread;
    uint64_t session_id;
    int nchildren;
    request_s *children[TABLE_SIZE];
};

#define PASSWORD "test"

static time_t base_time;
static uint64_t session_counter;

static pthread_mutex_t table_lock = PTHREAD_MUTEX_INITIALIZER;
static request_s *reqtable[TABLE_SIZE];

static bool isrunning;
static void *serve_client(void *arg);
static request_s *request_s_(int fd, struct sockaddr_in *client_ip);
static bool check_request(request_s *req);
static bool authenticate(request_s *req);
static bool pass_correct(char *pass);
static uint64_t new_session_id(void);
static void tx_new_session_id(request_s *req);
static void resolve_remote(request_s *req);

static void table_insert_request(request_s *req);
static void table_insert_request_(request_s *req, request_s *base[]);
static void table_delete_request(request_s *req);
static void table_delete_request_(request_s *req, request_s *base[]);
static void reparent(request_s *root);

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
    
    if(!check_request(req)) {
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
    int ip = client_ip->sin_addr.s_addr, i;
    request_s *req = del_alloc(sizeof *req);
    req->fd = fd;
    req->isactive = true;
    req->client_ip = *client_ip;
    
    for(i = 0; i < TABLE_SIZE; i++)
        req->children[i] = NULL;
    
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
    
    buf[BUF_SIZE - 1] = '\0';
    
    switch(buf[0]) {
        case PACKET_INIT:
            log_info("Testing: %s", &buf[1]);
            if(pass_correct(&buf[1])) {
                log_info("Login Success for [%s]", req->ipstr);
                tx_new_session_id(req);
                table_insert_request(req);
            }
            else {
                log_warn("Login Attempt failed for [%s]", req->ipstr);
            }
            break;
        case PACKET_REESTAB:
            
            break;
        default:
            //fail
            break;
    }
    
exit:
    //possible clean up code here(?)
    return false;
}

bool pass_correct(char *pass)
{
    int i;
    const char *pptr = PASSWORD;
    
    for(i = 0; i < BUF_SIZE; i++) {
        if(*pptr) {
            if(*pass == *pptr) {
                pass++;
                pptr++;
            }
        }
        else {
            break;
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
    
    buf[0] = PACKET_SESSIONID;
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
    enum {NFAILS = 50000, NINITS = 3};
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
    log_info("Read %lu bytes", (unsigned long)status);
    if(status < 0) {
        log_error("Failed to read on socket %d from client [%s]", req->fd, req->ipstr);
        return false;
    }
    
    if(buf[0] != PACKET_INIT) {
        int count = 0;
        while(buf[0] != PACKET_INIT) {
            log_info("val: %d\n", buf[0]);
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
            if(count == NINITS) {
                log_warn("Attempts Exhausted on socket %d for [%s]", req->fd, req->ipstr);
                return false;
            }
            else {
                count++;
                memset(buf, 0, sizeof(buf));
            }
        }
        log_info("GOT PACKET_INIT!");
    }
    
    buf[BUF_SIZE - 1] = '\0';
    
    log_info("Testing Password: %s", &buf[1]);
    
    if(strcmp(PASSWORD, &buf[1])) {
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


void table_insert_request(request_s *req)
{
    pthread_mutex_lock(&table_lock);
    table_insert_request_(req, reqtable);
    pthread_mutex_unlock(&table_lock);
}

void table_insert_request_(request_s *req, request_s *base[])
{
    request_s **prec =  &base[req->session_id % TABLE_SIZE],
                        *rec = *prec;
    
    if(rec) {
        if(req->session_id == rec->session_id) {
            log_error("Duplicate session_id detected. Illegal State.");
        }
        else {
            table_insert_request_(req, rec->children);
        }
    }
    else {
        *prec = rec;
    }
}


void table_delete_request(request_s *req)
{
    pthread_mutex_lock(&table_lock);
    table_delete_request_(req, reqtable);
    pthread_mutex_unlock(&table_lock);
}

void table_delete_request_(request_s *req, request_s *base[])
{
    int i;
    request_s **prec =  &base[req->session_id % TABLE_SIZE],
                        *rec = *prec;

    if(rec) {
        if(rec->session_id == req->session_id) {
            for(i = 0; i < TABLE_SIZE; i++) {
                if(rec->children[i]) {
                    reparent(rec->children[i]);
                }
            }
            free(rec);
            *prec = NULL;
        }
        else {
            table_delete_request_(req, rec->children);
        }
    }
}

void reparent(request_s *root)
{
    int i;
    
    for(i = 0; i < TABLE_SIZE; i++) {
        if(root->children[i]) {
            reparent(root->children[i]);
        }
        root->children[i] = NULL;
    }
    table_insert_request(root);
}
