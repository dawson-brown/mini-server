#include <sys/socket.h>
#include <pthread.h>
#include <netdb.h>
#include <signal.h>
#include <errno.h>
#include <poll.h>


enum sb_net_returns
{
    SB_SUCCESS,
    SB_ERR_GETADDRINFO,
    SB_ERR_SETSOCKOPT,
    SB_ERR_BIND,
    SB_ERR_LISTEN
};

enum sb_net_in_msgs
{
    SB_READY,
    SB_CLOSED_ON_ERR
};

enum sb_net_out_msgs
{
    SB_CLOSE
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
    int len;
    pthread_t * thread_ids;
    struct pollfd * pipes_in;
    struct pollfd * pipes_out;
};

struct sb_net_handler_ctx
{
    int sock_fd;
    int in_pipe;
    int out_pipe;
    struct sockaddr_storage client_addr;
    ssize_t ( *recv )(int, void *, size_t, int); //a (custom) recv function with same signature as recv
    ssize_t ( *send )(int, void *, size_t, int); //a (custom) send function with same signature as send
    
};

int sb_net_socket_setup(struct sb_net_server_info * server_info);

int sb_net_accept_conn(struct sb_net_server_info * server_info,
                        ssize_t ( *recv )(int, void *, size_t, int),
                        ssize_t ( *send )(int, void *, size_t, int));