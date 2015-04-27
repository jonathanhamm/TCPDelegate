#ifndef __TCPDelegate__tcp_delegate__
#define __TCPDelegate__tcp_delegate__

#include "general.h"


typedef enum {
    PACKET_OPEN = 1,
    PACKET_TX_SERVER = 2,
    PACKET_TX_CLIENT = 3,
    PACKET_CLOSE = 4,
    PACKET_SQL_QUERY = 5
} pack_type_e ;

typedef struct delegate_s delegate_s;
typedef struct datagram_s datagram_s;

extern delegate_s *delgate_init(const char *client_host, uint16_t client_port, const char *server_host, uint16_t server_port);


#endif /* defined(__TCPDelegate__TCPDelegate__) */
