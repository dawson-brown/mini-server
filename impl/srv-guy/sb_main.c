#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "sb_net.h"
#include "sb_main.h"

#define SB_BACKLOG 10
#define SB_PORT "8080"

int sb_req(str_t *req, void *app_data)
{
    return 0;
}

int sb_res(str_t *res, void *app_data)
{
    return 0;
}

int main(int argc, char **argv)
{
    // struct sb_net_server_info *server_info = sb_net_server_info_setup(
    //     SB_PORT,
    //     AF_UNSPEC,
    //     AI_PASSIVE,
    //     SOCK_STREAM,
    //     2,
    //     4,
    //     3);

    // sb_net_accept_conn(server_info, sb_req, sb_res, 32);

    // free(server_info);
    // return 0;
}