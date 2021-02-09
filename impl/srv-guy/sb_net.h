#include <sys/socket.h>
#include <netdb.h>
#include <signal.h>
#include <errno.h>


enum sb_net_returns
{
    SB_SUCCESS,
    SB_ERR_GETADDRINFO,
    SB_ERR_SETSOCKOPT,
    SB_ERR_BIND,
    SB_ERR_LISTEN
};

struct sb_net_server_info
{
    char * addr;
    char * port;
    int sock_fd;
    int addr_family;
    int addr_socktype;
    int addr_flags;
    int conn_backlog;
    int min_handlers;
    int max_handlers;
};

struct sb_net_handler_ctx
{
    int sock_fd;
    int in_pipe;
    int out_pipe;
    struct sockaddr_storage client_addr;
};

int sb_net_socket_setup(struct sb_net_server_info * server_info);

int sb_net_accept_conn(struct sb_net_server_info * server_info,
                        void *(*conn_handler) (void *));