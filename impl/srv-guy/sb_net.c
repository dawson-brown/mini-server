#include "sb_net.h"
#include "sb_net_internal.h"

static void * sb_net_set_arg(int conn_fd, struct sockaddr_storage * client_addr, void * common_arg, int sizeof_arg)
{
    void * arg;
    /*
    TODO: check that malloc doesn't fail
    */
    if (common_arg == NULL)
    {
        arg = malloc( sizeof(struct sb_net_handler_ctx) );
    }
    else
    {
        arg = malloc( sizeof(struct sb_net_handler_ctx) + sizeof_arg );
        memcpy(arg+sizeof(struct sb_net_handler_ctx), common_arg, sizeof_arg);
    }
    struct sb_net_handler_ctx * ctx = (struct sb_net_handler_ctx *) arg;
    ctx->sock_fd = conn_fd;
    ctx->client_addr = *client_addr;

    return arg;
}

int sb_net_server(struct sb_net_server_info * server_info, void *(*conn_handler) (void *), void * common_arg, int sizeof_arg)
{
    int sock_fd;
    int conn_fd;
    int ret;
    int reusable=1;
    socklen_t sin_size;
    // struct sigaction sa;
    char s[INET6_ADDRSTRLEN];
    void * arg;

    struct addrinfo hints, *servinfo, *p;
    struct sockaddr_storage client_addr;

    /*
    TODO: make server compatible with RAW
    for now SOCK_STREAM and DGRAM only
    */
    memset(&hints, 0, sizeof hints);
    hints.ai_family = server_info->addr_family;
    hints.ai_socktype = server_info->addr_socktype;
    hints.ai_flags = server_info->addr_flags; 

    /*
    https://man7.org/linux/man-pages/man3/getaddrinfo.3.html

    "getaddrinfo() returns one or more addrinfo structures,
       each of which contains an Internet address that can be specified
       in a call to bind(2) or connect(2)"

    "If the AI_PASSIVE flag is specified in hints.ai_flags, and node
    is NULL, then the returned socket addresses will be suitable for
    bind(2)ing a socket that will accept(2) connections."
    */
    if ((ret = getaddrinfo(NULL, server_info->port, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(ret));
        return 1;
    }

    for(p = servinfo; p != NULL; p = p->ai_next) 
    {
        if ((sock_fd = socket(p->ai_family, p->ai_socktype,p->ai_protocol)) == -1) 
        {
            perror("server: socket");
            continue;
        }

        /*
        https://linux.die.net/man/3/setsockopt
        SO_REUSEADDR : https://stackoverflow.com/questions/3229860/what-is-the-meaning-of-so-reuseaddr-setsockopt-option-linux

        "SO_REUSEADDR is most commonly set in network server programs, since a common usage pattern is to make a configuration change, then be required to restart that program to make the change take effect. Without SO_REUSEADDR, the bind() call in the restarted program's new instance will fail if there were connections open to the previous instance when you killed it."
        */
        if (setsockopt(sock_fd, SOL_SOCKET, SO_REUSEADDR, &reusable, sizeof(int)) == -1) 
        {
            perror("setsockopt");
            exit(1);
        }

        /*
        https://man7.org/linux/man-pages/man2/bind.2.html

        "When a socket is created with socket(2), it exists in a name
       space (address family) but has no address assigned to it.  bind()
       assigns the address specified by addr to the socket referred to
       by the file descriptor sockfd."
        */
        if (bind(sock_fd, p->ai_addr, p->ai_addrlen) == -1) 
        {
            close(sock_fd);
            perror("server: bind");
            continue;
        }

        break;
    }

    freeaddrinfo(servinfo);

    if (p == NULL) 
    {
        fprintf(stderr, "server: failed to bind\n");
        exit(1);
    }

    if (server_info->addr_socktype == SOCK_STREAM)
    {
        if (listen(sock_fd, server_info->conn_backlog) == -1) 
        {
            perror("listen");
            exit(1);
        }
        printf("server: waiting for connections...\n");


        while(1) 
        {
            sin_size = sizeof(client_addr);
            conn_fd = accept(sock_fd, (struct sockaddr *)&client_addr, &sin_size);

            if (conn_fd == -1) {
                perror("accept");
                continue;
            }
            inet_ntop(client_addr.ss_family, get_in_addr((struct sockaddr *)&client_addr), s, sizeof(s) );
            printf("server: got connection from %s\n", s);

            arg = sb_net_set_arg(sock_fd, &client_addr, common_arg, sizeof_arg);

            pthread_t thread;;
            int thread = pthread_create(&thread, NULL, conn_handler, arg);
        }
    }
    else 
    {
        printf("listener: waiting to recvfrom...\n");
        arg = sb_net_set_arg(sock_fd, &client_addr, common_arg, sizeof_arg);

        conn_handler(arg);
    }
}