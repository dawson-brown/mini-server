#include "sb_net.h"
#include "sb_net_internal.h"

#define SB_DEFAULT_BUFFER 256

/***********************************************************
 * Static functions: start
 **********************************************************/
inline static void swap_n_bytes(void * a, void * b, int n)
{
    unsigned char tmp[n];
    memcpy(tmp, a, n);
    memcpy(a, b, n);
    memcpy(b, &tmp, n);
}

static void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) 
    {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

static void safe_close(int socket)
{
    ssize_t len;
    int ret;
    ret = shutdown(socket, SHUT_RDWR);
    if (ret == -1)
    {
        perror("safe_close - shutdown");
        return;
    }

    int msg;
    while (1)
    {
        len = read(socket, &msg, sizeof(int));
        if ( (len==-1) || (len==0) )
        {
            break;
        }
    }

    ret = close(socket);
    if (ret==-1)
    {
        perror("safe_close - close");
    }
}

static struct sb_net_handler_ctx * sb_setup_thread_pipes(struct sb_net_server_info * server, int threads_i)
{
    int pipe_in[2];
    int pipe_out[2];

    if (pipe(pipe_in) == -1 || pipe(pipe_out) == -1 )
    {
        perror("pipe");
        return NULL;
    }
    
    struct sb_net_handler_ctx * ctx = malloc(sizeof(struct sb_net_handler_ctx)); 

    ctx->in_pipe = pipe_out[0];
    server->pipes_out[threads_i].fd = pipe_out[1];
    server->pipes_out[threads_i].events = POLLIN;

    ctx->out_pipe = pipe_in[1];
    server->pipes_in[threads_i].fd = pipe_in[0];
    server->pipes_in[threads_i].events = POLLIN;

    return ctx;
}

static void * sb_net_thread(void * ctx)
{   
    /*
    when started, check that sockfd is below zero
        - if so, send message to main loop saying you're ready.
        - otherwise deal with sockfd (which is open connection)
    */
    struct sb_net_handler_ctx * conn_ctx = (struct sb_net_handler_ctx *)ctx;
    ssize_t len;

    if (conn_ctx->sock_fd == -1)
    {
        int ready_msg = SB_READY;
        len = write(conn_ctx->out_pipe, &ready_msg, sizeof(int));
        if (len == -1)
        {
            perror("sb_net_thread - write");
        }
    }

    char * req_buffer = malloc(SB_DEFAULT_BUFFER);
    ssize_t req_len = SB_DEFAULT_BUFFER;
    char * res_buffer = malloc(SB_DEFAULT_BUFFER);
    ssize_t res_len = SB_DEFAULT_BUFFER;

    while (1)
    {
        int msg;
        len = read(conn_ctx->in_pipe, &msg, sizeof(int));
        if (len == -1)
        {
            perror("sb_net_thread - read:");
            exit(0);
        }

        req_len = conn_ctx->recv(msg, req_buffer, req_len, 0);
        res_len = conn_ctx->process(req_buffer, req_len, res_buffer, res_len);
        len = conn_ctx->send(msg, res_buffer, res_len, 0);
        sleep(1);

        //safely close connection
        safe_close(msg);

        int ready_msg = SB_READY;
        len = write(conn_ctx->out_pipe, &ready_msg, sizeof(int));
    }
    
    return NULL;
}

static int sb_launch_thread(
    int i,
    int sock_fd, 
    struct sb_net_server_info *server_info,
    ssize_t (*custom_recv)(int, void *, size_t, int),
    ssize_t (*custom_send)(int, void *, size_t, int),
    ssize_t ( *custom_process )(char * req, ssize_t req_len, char * res, ssize_t res_len)
)
{
    int ret;
    struct sb_net_handler_ctx * ctx = sb_setup_thread_pipes(server_info, i); 
    if (ctx == NULL)
    {
        return SB_ERR_PIPE;
    }
    ctx->sock_fd = sock_fd;
    ctx->recv = custom_recv;
    ctx->send = custom_send;
    ctx->process = custom_process;
    ret = pthread_create(&server_info->thread_ids[i], NULL, sb_net_thread, ctx);
    if (ret != 0)
    {
        return SB_ERR_PTHREAD;
    }

    return SB_SUCCESS;
}

static int sb_net_socket_setup(struct socket_info * socket_info)
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
    hints.ai_family = socket_info->addr_family;
    hints.ai_socktype = socket_info->addr_socktype;
    hints.ai_flags = socket_info->addr_flags; 

    /*
    https://man7.org/linux/man-pages/man3/getaddrinfo.3.html

    "getaddrinfo() returns one or more addrinfo structures,
       each of which contains an Internet address that can be specified
       in a call to bind(2) or connect(2)"

    "If the AI_PASSIVE flag is specified in hints.ai_flags, and node
    is NULL, then the returned socket addresses will be suitable for
    bind(2)ing a socket that will accept(2) connections."
    */
    if ((ret = getaddrinfo(NULL, socket_info->port, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(ret));
        return SB_ERR_GETADDRINFO;
    }

    for(p = servinfo; p != NULL; p = p->ai_next) 
    {
        if ((sock_fd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) 
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

    memcpy(&socket_info->addr, p->ai_addr, sizeof(struct sockaddr));
    socket_info->addr_len = p->ai_addrlen;
    socket_info->sock_fd = sock_fd;
    return SB_SUCCESS;

}

/***********************************************************
 * Static functions: end
 **********************************************************/


struct sb_net_server_info * sb_net_server_info_setup(
    const char * port,
    const int addr_family,
    const int addr_flags,
    const int addr_socktype,
    const int conn_backlog,
    const int max_handlers,
    const int min_handlers
)
{
    struct sb_net_server_info * server_info = malloc(sizeof(struct sb_net_server_info));
    strcpy(server_info->socket.port, port);
    server_info->socket.addr_family = addr_family;
    server_info->socket.addr_flags = addr_flags;
    server_info->socket.addr_socktype = addr_socktype;
    server_info->conn_backlog = conn_backlog;
    server_info->max_handlers = max_handlers;
    server_info->min_handlers = min_handlers;
	server_info->len = 0;
    server_info->current_thread = 0;

    int ret = sb_net_socket_setup(&server_info->socket);
    if (ret != SB_SUCCESS)
    {
        //TODO: have better error handling--sb_net_socket_setup can fail more many reasons
        printf("sb_net_socket_setup failed: %d\n", ret);
        return NULL;
    }

    return server_info;
}

int sb_net_accept_conn(struct sb_net_server_info * server_info, 
                        ssize_t ( *custom_recv )(int, void *, size_t, int),
                        ssize_t ( *custom_send )(int, void *, size_t, int),
                        ssize_t ( *req_processor )(char * req, ssize_t req_len, char * res, ssize_t res_len))
{
    int sock_fd = server_info->socket.sock_fd;
    int conn_fd;
    struct sockaddr_storage client_addr;
    socklen_t sin_size;
    char s[INET6_ADDRSTRLEN];

    ssize_t len;

    if (listen(sock_fd, server_info->conn_backlog) == -1) 
    {
        printf("sock_fd: %d\n", sock_fd);
        perror("listen");
        return SB_ERR_LISTEN;
    }
    printf("sb_net_accept_conn - server: waiting for connections...\n");

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
    struct pollfd pipes_in[server_info->max_handlers];
    struct pollfd pipes_out[server_info->max_handlers];
    server_info->thread_ids = thread_ids;
    server_info->pipes_in = pipes_in;
    server_info->pipes_out = pipes_out;

    for (int i=0; i<server_info->min_handlers; i++)
    {    
        if (sb_launch_thread(i, -1, server_info, custom_recv, custom_send, req_processor) != 0)
        {
            //TODO: actually handle errors
            printf("thread launch failed...exiting\n");
            exit(1);
        }
        server_info->len++;
    }
    
    int i;
    int ready;
    int thread_count;
    int pipe_count;
    int msg;
    // int thread_err;
    while(1)
    /*
    When accept is called and conn_fd is set, send conn_fd as a pipe argument
    */
    {
        //iterate over ready polls--check for POLLIN
        //if POLLIN, read from pipe--if ready, keep array of ready threads
        //array of ready threads might have to be a seperate list of those threads that are ready.
        //the ready threads will be the out pipes that can be written to. --so for each ready thread, copy the correct out pipe to the ready array 
        //remember: when doing the swapping to the back thing when iterating over poll results, make sure to keep the out pipes, in pipes, and threads inline. 

        /**
         * see which pipes can be read from--put all ready in front of pollfds
        */
        i = 0;
        ready = poll(pipes_in, server_info->len, 0);
        pipe_count = 0;
        while (pipe_count < ready)
        {
            if (pipes_in[i].revents & POLLIN){
                swap_n_bytes(&pipes_in[i], &pipes_in[pipe_count], sizeof(struct pollfd));
                swap_n_bytes(&pipes_out[i], &pipes_out[pipe_count], sizeof(struct pollfd));
                swap_n_bytes(&thread_ids[i], &thread_ids[pipe_count], sizeof(pthread_t));
                pipe_count++;
            }
            i++;
        } 

        //read from each ready pipe to see which thread sent the 
        // "I'm ready" message (SB_READY)
        i=0;
        ready = pipe_count;
        thread_count = 0;
        // thread_err = 0;
        while (i<ready)
        {
            len = read(pipes_in[i].fd, &msg, sizeof(int));
            if (len == -1)
            {
                /* TODO: handle read error */
                perror("read");
                exit(0);
            }

            if (msg == SB_READY)
            {
                swap_n_bytes(&pipes_in[i], &pipes_in[thread_count], sizeof(struct pollfd));
                swap_n_bytes(&pipes_out[i], &pipes_out[thread_count], sizeof(struct pollfd));
                swap_n_bytes(&thread_ids[i], &thread_ids[thread_count], sizeof(pthread_t));
                thread_count++;
            }
            i++;
        }

        if (thread_count != pipe_count)
        {
            //TODO: handle errors
            //if above condition is true, a thread sent a message other than SB_READY
        }

        while (thread_count || server_info->len<server_info->max_handlers)
        {
            //TODO: add timeout on sock_fd--if no incoming connection,
            //then its fine to check if any threads have become ready while waiting.
            sin_size = sizeof(client_addr);
            conn_fd = accept(sock_fd, (struct sockaddr *)&client_addr, &sin_size);
            if (conn_fd == -1) 
            {
                //TODO: handle errors
                perror("accept");
                continue;
            }
            inet_ntop(client_addr.ss_family, get_in_addr((struct sockaddr *)&client_addr), s, sizeof(s) );
            printf("sb_net_accept_conn - server: got connection from %s\n", s);

            if (thread_count==0) 
            {
                if (server_info->len<server_info->max_handlers)
                {
                    if (sb_launch_thread(server_info->len, conn_fd, server_info, custom_recv, custom_send, req_processor) != 0)
                    {
                        //TODO: actually handle errors
                        printf("thread launch failed...exiting\n");
                        exit(1);
                    }
                    server_info->len++;
                }
                else 
                {
                    //pass connections to pipes_out (pass to ready_threads-1);
                    len = write(pipes_out[server_info->current_thread].fd, &conn_fd, sizeof(int));
                    server_info->current_thread=(server_info->current_thread+1) % server_info->len;
                    if (len == -1)
                    {
                        //TODO: handle errors
                        perror("write");
                        exit(1);
                    }
                }
            }
            else
            {
                //pass connections to pipes_out (pass to ready_threads-1);
                //decrement ready_threads
                len = write(pipes_out[thread_count-1].fd, &conn_fd, sizeof(int));
                thread_count--;
                if (len == -1)
                {
                    //TODO: handle errors
                    perror("write");
                    exit(1);
                }
            }
        };

        //TODO: mechanism for closing threads if len exceeds min

    }
}