#include "tcp_delegate.h"

typedef struct pck_open_s pck_open_s;

struct delegate_s {
    uint16_t client_port;
    const char *client_host;
    uint16_t server_port;
    const char *server_host;
    pthread_t thread;
    
    time_t last_in;
    
    void (*keep_alive)(void);
};

struct datagram_s {
    pack_type_e type;
    void *data;
};

struct pck_open_s {
    const char host[4];
    uint16_t port;
};

static void *run_loop(void *arg);

delegate_s *delgate_init(const char *client_host, uint16_t client_port, const char *server_host, uint16_t server_port)
{
    delegate_s *d = del_alloc(sizeof *d);
    d->client_host = client_host;
    d->client_port = client_port;
    d->server_host = server_host;
    d->server_port = server_port;
    return d;
}

void *run_loop(void *arg)
{
    delegate_s *d = arg;
    
    
    
    pthread_exit(NULL);
}


