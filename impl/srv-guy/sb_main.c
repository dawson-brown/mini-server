#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "sb_net.h"
#include "sb_main.h"

#define SB_BACKLOG 10
#define SB_PORT "8080"

void * sb_conn_handler(void * ctx)
{
    return NULL;
}

int main(int argc, char ** argv)
{   
    struct sb_net_server_info server_info;
    server_info.addr = NULL;
    server_info.port = SB_PORT;
    server_info.addr_family = AF_UNSPEC;
    server_info.addr_flags = AI_PASSIVE;
    server_info.addr_socktype = SOCK_STREAM;
    server_info.conn_backlog = SB_BACKLOG;
    server_info.max_handlers = 10;
    server_info.min_handlers = 1;

    int ret;
    if ( ( ret = sb_net_socket_setup(&server_info) ) != SB_SUCCESS )
    {
        printf("sb_net_server failed...\n");
        exit(1);
    }

    sb_net_accept_conn(&server_info, sb_conn_handler);

    return 0;

}