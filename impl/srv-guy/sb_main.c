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

    sb_net_server(&server_info, sb_conn_handler, NULL, 0);
    return 0;

}