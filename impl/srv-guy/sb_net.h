#include <sys/socket.h>
#include <pthread.h>
#include <netdb.h>
#include <signal.h>
#include <errno.h>
#include <poll.h>
#include <fcntl.h>

#define BUFFER_DEFAULT_LEN 256

typedef struct str {
    char * buffer;
    ssize_t buf_len;
    ssize_t data_len;
} str_t;

str_t buffer_init();

void buffer_reset(str_t * str);

void buffer_resize(str_t * str, ssize_t new_length);

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
    SB_APP_RECV,
    SB_APP_SEND,
    SB_APP_CONN_DONE
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

struct thread_comm {
    pthread_t * thread_ids;
    struct pollfd * pipes_in;
    struct pollfd * pipes_out;
};

struct sb_net_server_info
{
    struct socket_info socket;
    int conn_backlog;
    int min_handlers;
    int max_handlers;
    int len;
    int current_thread;
    struct thread_comm threads;
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
    int ( *process_req )(str_t req, void * app_data);
    int ( *process_res )(str_t * res, void * app_data);
};

#ifndef MINUNIT_MINUNIT_H
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
                        int ( *process_req )(str_t req, void * app_data),
                        int ( *process_res )(str_t * res, void * app_data),
                        void ( *init_app_data)(void * app_data),
                        const int app_data_size);
#endif
