#include <sys/socket.h>
#include <netdb.h>
#include <signal.h>
#include <errno.h>

struct sb_net_server_info
{
    char * addr;
    char * port;
    int addr_family;
    int addr_socktype;
    int addr_flags;
    int conn_backlog;
    int max_handlers;
};

struct sb_net_handler_ctx
{
    int sock_fd;
    struct sockaddr_storage client_addr;
};

int sb_net_server(struct sb_net_server_info * server_info, void *(*conn_handler) (void *), void * arg, int sizeof_arg);