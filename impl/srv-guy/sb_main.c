#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "sb_net.h"
#include "sb_main.h"

#define SB_BACKLOG 10
#define SB_PORT "8080"

ssize_t sb_recv(int socket, void * buf, size_t n, int flags) 
{
    return 0;
}

ssize_t sb_send(int socket, void * buf, size_t n, int flags) 
{
    return 0;
}

ssize_t sb_req_processor(char * req, ssize_t req_len, char * res, ssize_t res_len)
{
    return 0;
}

int main(int argc, char ** argv)
{   
    struct sb_net_server_info * server_info = sb_net_server_info_setup(
		SB_PORT,
		AF_UNSPEC,
		AI_PASSIVE,
		SOCK_STREAM,
		SB_BACKLOG,
		10,
		1
	);

    int ret;
    if ( ( ret = sb_net_socket_setup(server_info) ) != SB_SUCCESS )
    {
        printf("sb_net_server failed...\n");
        exit(1);
    }

    sb_net_accept_conn(&server_info, sb_recv, sb_send, sb_req_processor);

    free(server_info);
    return 0;

}