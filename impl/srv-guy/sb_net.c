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

static void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) 
    {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

int sb_net_socket_setup(struct sb_net_server_info * server_info)
{
    int sock_fd;
    int ret;
    int reusable=1;
    // struct sigaction sa;
    struct addrinfo hints, *servinfo, *p;

    /*
    TODO: make server compatible with RAW
    for now STREAM and DGRAM only
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
        return SB_ERR_GETADDRINFO;
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
            return SB_ERR_SETSOCKOPT;
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
        return SB_ERR_BIND;
    }

    server_info->sock_fd = sock_fd;
    return SB_SUCCESS;

}

int sb_net_accept_conn(struct sb_net_server_info * server_info, 
                        void *(*conn_handler) (void *))
{
    int sock_fd = server_info->sock_fd;
    int conn_fd;
    struct sockaddr_storage client_addr;
    socklen_t sin_size;
    char s[INET6_ADDRSTRLEN];
    
    if (listen(sock_fd, server_info->conn_backlog) == -1) 
    {
        perror("listen");
        return SB_ERR_LISTEN;
    }
    printf("server: waiting for connections...\n");

    /*
    see pipe and poll:
        https://stackoverflow.com/questions/25876805/c-how-to-make-threads-communicate-with-each-other
        https://man7.org/linux/man-pages/man2/pipe.2.html
        https://man7.org/linux/man-pages/man7/pipe.7.html
        https://man7.org/linux/man-pages/man2/poll.2.html

    there should be two distinct functions here--the connection handler that receives incoming data and
    a user defined function to handle the data. The connection handler is defined as part of this sb_net
    library. The connection handler passes the data to the user defined handler.

    For each thread, there will be 2 pipes--one to the thread and one to this accept function. Server info will 
    have a min and max number of threads set. This function will always maintain the min number of threads
    and will fire up to the max number of threads as needed. The threads will send a message to this function to
    say they can handle a connection, this function will poll those descriptors and will send incoming connections
    only to those threads that are ready. When the threads are first setup they say they're ready and when they finish
    a connection they'll say they're ready. It will remain in this 'send to existing thread' mode until it hits a point 
    that there are no available threads. Then it will create a single new thread for the new connection and add 
    that thread to descriptors/pipes being polled/maintained. It will then revert back to 'poll mode'--this should of 
    course return the new thread (and maybe others) if the new thread was setup right.

    (the last two sentences above might be bad. This function might get scheduled before the new thread does and so the
    new thread won't have sent its 'im ready' message yet. Maybe there should be an op_mode to the ctx and its possible
    to send the conn_fd as an argument to the thread when its created, and if a conn_fd is sent, that thread won't send its ready
    message until after its done with that argument fd.)

    If there comes a point when poll returns descriptors that are ready, but there are no waiting connections and 
    more than min threads are open, send 'close' messages to one of the threads and poll again. If there are still no
    connections and still more than min threads, close another. Keep doing this, one at a time, until there are min 
    threads. There will have to be a simple counter for the number of open threads.

    */

    //min threads setup;
    pthread_t thread_ids[server_info->max_handlers];
    int pipes_in[server_info->max_handlers];
    int pipes_out[server_info->max_handlers];
    struct thread_tracker threads = {thread_ids, pipes_out, pipes_in, server_info->max_handlers};

    int pipe_in[2];
    int pipe_out[2];

    for (int i=0; i<threads.len; i++)
    {
        if (pipe(pipe_in) == -1 || pipe(pipe_out) == -1 )
        {
            perror("pipe");
            exit(1);
        }
    
        struct sb_net_handler_ctx * ctx = malloc(sizeof(struct sb_net_handler_ctx)); 
        ctx->client_addr = client_addr;

        //setup pipes--this can probably be a function
        ctx->in_pipe = pipe_out[0];
        threads.pipes_out[i] = pipe_out[1];

        ctx->out_pipe = pipe_in[1];
        threads.pipes_in[i] = pipe_in[0];

        int thread = pthread_create(&threads.thread_ids[i], NULL, conn_handler, &ctx);
    }
    
    while(1)
     /*
    rework this loop. Must add loop to add min threads.
    When accept is called and conn_fd is set, send conn_fd as a pipe argument
    */
    {
        sin_size = sizeof(client_addr);
        conn_fd = accept(sock_fd, (struct sockaddr *)&client_addr, &sin_size);

        if (conn_fd == -1) 
        {
            perror("accept");
            continue;
        }
        inet_ntop(client_addr.ss_family, get_in_addr((struct sockaddr *)&client_addr), s, sizeof(s) );
        printf("server: got connection from %s\n", s);

        //pass connections;
        //poll for ready threads;
    }
}

void * sb_net_recv(void * ctx)
{
    struct sb_net_handler_ctx * conn_ctx = (struct sb_net_handler_ctx *)ctx;
    return NULL;
}