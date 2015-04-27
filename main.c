#include "general.h"
#include "server.h"

int main(int argc, const char *argv[])
{
    log_init();
    
    if(argc == 1) {
        server_start(DEFAULT_PORT);
    }
    else if(argc == 2) {
        uint16_t port = (uint16_t)atoi(argv[1]);
        server_start(port);
    }
    else {
        log_error("Invalid number of args supplied on startup");
    }
    
    log_deinit();
    return 0;
}
