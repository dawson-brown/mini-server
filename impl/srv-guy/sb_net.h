#include <sys/socket.h>
#include <pthread.h>
#include <netdb.h>
#include <signal.h>
#include <errno.h>
#include <poll.h>
#include <fcntl.h>


enum sb_net_returns
{
    SB_SUCCESS,
    SB_ERR_GETADDRINFO,
    SB_ERR_SETSOCKOPT,
    SB_ERR_FCNTL,
    SB_ERR_BIND,
    SB_ERR_LISTEN,
    SB_ERR_PIPE,
    SB_ERR_PTHREAD
};

enum sb_net_pipe_msgs
{
    SB_CLOSED_ON_ERR = -2,
    SB_CLOSE,
    SB_READY = 0
};

enum sb_net_app_flags {
    SB_APP_RECV, //this indicates a 'fresh' recv--whats been recieved can be overwritten
    SB_APP_RECV_MORE, //this indicates that whats recieved next should be appended to
    SB_APP_SEND,
    SB_APP_SEND_MORE,
    SB_APP_ERR, // this is an error
    SB_APP_CLOSE_CONN,
    SB_APP_SESSION_DONE,
    /* TODO: figure out the proper return flags.
    maybe make them powers of 2 to be ORed together? */
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

    /**
     * the user defined application layer.
     */
    void * app_data; 
    int ( *process_req )(const char * req, const ssize_t req_len, char ** res, ssize_t * res_len, void * app_data);
    int ( *process_res )(char * res, ssize_t res_len, void * app_data);
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
                        int ( *process_req )(const char * req, const ssize_t req_len, char ** res, ssize_t * res_len, void * app_data),
                        int ( *process_res )(char * res, ssize_t res_len, void * app_data),
                        const int app_data_size);