#include "sb_net.h"
#include "sb_net_internal.h"

// static void * sb_net_set_arg(int conn_fd, struct sockaddr_storage * client_addr, void * common_arg, int sizeof_arg)
// {
//     void * arg;
//     /*
//     TODO: check that malloc doesn't fail
//     */
//     if (common_arg == NULL)
//     {
//         arg = malloc( sizeof(struct sb_net_handler_ctx) );
//     }
//     else
//     {
//         arg = malloc( sizeof(struct sb_net_handler_ctx) + sizeof_arg );
//         memcpy(arg+sizeof(struct sb_net_handler_ctx), common_arg, sizeof_arg);
//     }
//     struct sb_net_handler_ctx * ctx = (struct sb_net_handler_ctx *) arg;
//     ctx->sock_fd = conn_fd;
//     ctx->client_addr = *client_addr;

//     return arg;
// }

/***********************************************************
 * Static functions: start
 **********************************************************/

inline static void swap_fds(struct pollfd * a, struct pollfd * b)
{
    struct pollfd tmp = *a;
    memcpy(a, b, sizeof(struct pollfd));
    memcpy(b, &tmp, sizeof(struct pollfd));
}

inline static void swap_pthreads(pthread_t * a, pthread_t * b)
{
    pthread_t tmp = *a;
    memcpy(a, b, sizeof(pthread_t));
    memcpy(b, &tmp, sizeof(pthread_t));
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

/***********************************************************
 * Static functions: end
 **********************************************************/


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

void * sb_net_thread(void * ctx)
{   
    /*
    when started, check that sockfd is below zero
        - if so, send message to main loop saying you're ready.
        - otherwise deal with sockfd (which is open connection)
    */
    struct sb_net_handler_ctx * conn_ctx = (struct sb_net_handler_ctx *)ctx;
    ssize_t len;
    printf("sb_net_thread - ctx: in:%d, out:%d\n", conn_ctx->in_pipe, conn_ctx->out_pipe);

    if (conn_ctx->sock_fd == -1)
    {
        int ready_msg = SB_READY;
        len = write(conn_ctx->out_pipe, &ready_msg, sizeof(int));
        if (len == -1)
        {
            perror("sb_net_thread - write");
        }
    }

    while (1)
    {
        int msg;
        len = read(conn_ctx->in_pipe, &msg, sizeof(int));
        if (len == -1)
        {
            perror("sb_net_thread - read:");
            exit(0);
        }

        //TODO: handle connection
        
        //safely close connection
        safe_close(msg);

        int ready_msg = SB_READY;
        len = write(conn_ctx->out_pipe, &ready_msg, sizeof(int));
    }
    
    return NULL;
}

struct sb_net_handler_ctx * sb_setup_threads(struct thread_tracker * threads, int threads_i)
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
    threads->pipes_out[threads_i].fd = pipe_out[1];
    threads->pipes_out[threads_i].events = POLLIN;

    ctx->out_pipe = pipe_in[1];
    threads->pipes_in[threads_i].fd = pipe_in[0];
    threads->pipes_in[threads_i].events = POLLIN;

    printf("sb_setup_threads - ctx: in:%d, out:%d\n", ctx->in_pipe, ctx->out_pipe);
    printf("sb_setup_threads - thread: in:%d, out:%d\n", threads->pipes_in[threads_i].fd, threads->pipes_out[threads_i].fd);

    return ctx;
}

int sb_net_accept_conn(struct sb_net_server_info * server_info, 
                        ssize_t ( *custom_recv )(int, void *, size_t, int),
                        ssize_t ( *custom_send )(int, void *, size_t, int))
{
    int sock_fd = server_info->sock_fd;
    int conn_fd;
    struct sockaddr_storage client_addr;
    socklen_t sin_size;
    char s[INET6_ADDRSTRLEN];

    ssize_t len;
    int ret;

    if (listen(sock_fd, server_info->conn_backlog) == -1) 
    {
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
    struct thread_tracker threads = {thread_ids, pipes_out, pipes_in, server_info->min_handlers};

    for (int i=0; i<threads.len; i++)
    {    
        struct sb_net_handler_ctx * ctx = sb_setup_threads(&threads, i); 
        if (ctx == NULL)
        {
            exit(1);
        }
        ctx->sock_fd = -1;
        ctx->recv = custom_recv;
        ctx->send = custom_send;
        printf("sb_net_accept_conn - ctx: in:%d, out:%d\n", ctx->in_pipe, ctx->out_pipe);
        ret = pthread_create(&threads.thread_ids[i], NULL, sb_net_thread, ctx);
        if (ret != 0)
        {
            printf("sb_net_accept_conn - pthread_create: %d\n", ret);
        }
    }
    
    int i;
    int ready;
    int ready_threads;
    int msg;
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

        i = 0;
        ready = poll(pipes_in, threads.len, 0);
        ready_threads = 0;

        /**
         * see which pipes can be read from--put all ready in front of pollfds
        */ 
        while (ready)
        {
            if (pipes_in[i].revents & POLLIN)
            {
                i++;
                ready--;
                ready_threads++;
            } 
            else 
            {
                //make sure in, out and threads remain lined up with eachother.
                swap_fds(&pipes_in[i], &pipes_in[threads.len-1]);
                swap_fds(&pipes_out[i], &pipes_out[threads.len-1]);
                swap_pthreads(&thread_ids[i], &thread_ids[threads.len-1]);
            }
        }

        //read from each ready pipe to see which thread sent the 
        //I'm ready message (SB_READY)
        i=0;
        ready = ready_threads;
        while (ready)
        {
            len = read(pipes_in[i].fd, &msg, sizeof(int));

            if (msg == SB_READY)
            {
                i++;
                ready--;
                continue;
            }
            else
            {
                swap_fds(&pipes_in[i], &pipes_in[ready_threads-1]);
                swap_fds(&pipes_out[i], &pipes_out[ready_threads-1]);
                swap_pthreads(&thread_ids[i], &thread_ids[ready_threads-1]);
                ready_threads-=1;
                ready-=1;
                if (msg == SB_CLOSED_ON_ERR)
                {
                    threads.len-=1;
                    if (threads.len < server_info->min_handlers)
                    {
                        //TODO:
                        //fire up new thread at position: ready_threads-1
                        //will use same ctx as the one that crashed--ie set the pipes appropriately.
                    }
                }
            }
        }

        //TODO: if there are no ready threads, fire up another thread

        while (ready_threads)
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

            //pass connections to pipes_out (pass to ready_threads-1);
            //decrement ready_threads
            len = write(pipes_out[ready_threads-1].fd, &conn_fd, sizeof(int));
            ready_threads--;
            if (len == -1)
            {
                //TODO: handle errors
                perror("write");
                continue;
            }
        };

    }
}