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
    SB_ERR_LISTEN,
    SB_ERR_PIPE,
    SB_ERR_PTHREAD
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

struct socket_info
{
    socklen_t addr_len;
    struct sockaddr addr;
    char port[6];
    int sock_fd;
    int addr_family;
    int addr_socktype;
    int addr_flags; 
};

struct sb_net_server_info
{
    struct socket_info socket;
    int conn_backlog;
    int min_handlers;
    int max_handlers;
    int len;
    int current_thread;
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
    ssize_t ( *process )(char * req, ssize_t req_len, char * res, ssize_t res_len); //a request processor that places the response into res
};

struct sb_net_server_info * sb_net_server_info_setup(
    const char * port,
    const int addr_family,
    const int addr_flags,
    const int addr_socktype,
    const int conn_backlog,
    const int max_handlers,
    const int min_handlers
);

int sb_net_accept_conn(struct sb_net_server_info * server_info, 
                        ssize_t ( *custom_recv )(int, void *, size_t, int),
                        ssize_t ( *custom_send )(int, void *, size_t, int),
                        ssize_t ( *custom_process )(char * req, ssize_t req_len, char * res, ssize_t res_len));