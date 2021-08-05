#include "sb_net.h"
#include "sb_net_internal.h"

#define SB_DEFAULT_BUFFER 256

str_t buffer_init()
{
    str_t str;
    str.buffer = malloc(BUFFER_DEFAULT_LEN);
    str.buf_len = BUFFER_DEFAULT_LEN;
    str.data_len = 0;
    return str;
}

void buffer_reset(str_t * str)
{
    if (str->buf_len == BUFFER_DEFAULT_LEN) return;

    str->buffer = realloc(str->buffer, BUFFER_DEFAULT_LEN);
    str->buf_len = BUFFER_DEFAULT_LEN;
    str->data_len = 0;
}

void buffer_resize(str_t * str, ssize_t new_length)
{
    str->buffer = realloc(str->buffer, new_length);
    str->buf_len = new_length;
}


/***********************************************************
 * Static functions: start
 **********************************************************/

/**
 * swap \p n bytes in \p a and \p b
 * 
 * \param[out] a an address
 * \param[out] b an address
 * \param[in] n the number of bytes to swap
 */ 
inline static void swap_n_bytes(void * a, void * b, int n)
{
    unsigned char tmp[n];
    memcpy(tmp, a, n);
    memcpy(a, b, n);
    memcpy(b, &tmp, n);
}

/**
 * this function takes a pointer to a struct thread_comm structure and swaps
 * members of all three arrays at i and j. This function is necessary to keep pipes_in,
 * pipes_out, and thread_ids always in alignment.
 * 
 * \param[out] a a thread_comm structure
 * \param[in] i an index into the arrays belonging to \p a
 * \param[in] j an index into the arrays belonging to \p a
 * 
 */ 
inline static void swap_thread_comm_structs(struct thread_comm * a, int i, int j)
{
    swap_n_bytes(&a->pipes_in[i], &a->pipes_in[j], sizeof(struct pollfd));
    swap_n_bytes(&a->pipes_out[i], &a->pipes_out[j], sizeof(struct pollfd));
    swap_n_bytes(&a->thread_ids[i], &a->thread_ids[j], sizeof(pthread_t));
}

static void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) 
    {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

static void safe_close(int sock)
{
    ssize_t len;
    int ret;
    ret = shutdown(sock, SHUT_RDWR);
    if (ret == -1)
    {
        perror("safe_close - shutdown");
        return;
    }

    int msg;
    while (1)
    {
        len = read(sock, &msg, sizeof(int));
        if ( (len==-1) || (len==0) )
        {
            break;
        }
    }

    ret = close(sock);
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
    server->threads.pipes_out[threads_i].fd = pipe_out[1];
    server->threads.pipes_out[threads_i].events = POLLIN;

    ctx->out_pipe = pipe_in[1];
    server->threads.pipes_in[threads_i].fd = pipe_in[0];
    server->threads.pipes_in[threads_i].events = POLLIN;

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
    int sock = conn_ctx->sock_fd;

    str_t req = buffer_init(SB_DEFAULT_BUFFER);

    ssize_t sent_len;
    str_t res = buffer_init(SB_DEFAULT_BUFFER);

    int ret = SB_APP_RECV;
    int message;

    if (sock == -1)
    {
        message = SB_READY;
        len = write(conn_ctx->out_pipe, &message, sizeof(int));
        if (len == -1)
        {
            perror("sb_net_thread - write");
        }
    }

    while (1) 
    {
        if (sock == -1)
        {
            len = read(conn_ctx->in_pipe, &message, sizeof(int));
            if (len == -1)
            {
                //TODO: handle errors
                perror("sb_net_thread - read:");
                exit(0);
            }

            if (message > 0)
            {
                sock = message;
            }
            else 
            {
                if (message == SB_CLOSE)
                {
                    goto thread_done;
                }
            }
        }

        /*
            RECIEVE
            TODO: if req.data_len == req.buffer_len maybe resize and recv again?
                because that will mean that it filled the buffer and there is 
                probably still more to recv
                buffer_resize(&req, req.buf_len*2);

                under what conditions should buffer_reset(&req); ? assuming the buffer
                is resized at all.

                recv and send will have to be non-blocking...
        */
        if (ret == SB_APP_RECV)
        {
            req.data_len = recv(sock, req.buffer, req.buf_len, 0);
            if (req.data_len == -1)
            {
                // TODO: handle errors...
                perror("recv");
                goto thread_done;
            }
            ret = conn_ctx->process_req(req, conn_ctx->app_data);
        }
        
        /*
            SEND
        */
        if (ret == SB_APP_SEND)
        {
            ret = conn_ctx->process_res(&res, conn_ctx->app_data); 
            sent_len = 0;

            do {
                len = send(sock, res.buffer + sent_len, res.data_len - sent_len, 0);

                if (len == -1)
                {
                    // TODO: handle errors...
                    perror("send");
                    goto thread_done;
                }
                sent_len += len;
            } while (sent_len < res.data_len);
        }        

        if (ret == SB_APP_CONN_DONE)
        {
            //safely close connection
            safe_close(sock);
            buffer_reset(&res);
            buffer_reset(&req);
            sock = -1;
        }

    }

    thread_done:
    close(conn_ctx->out_pipe);
    close(conn_ctx->in_pipe);

    free(req.buffer);
    free(res.buffer);
    free(ctx);
    free(conn_ctx->app_data);
    
    return NULL;
}

static int sb_launch_thread(
    int i,
    int sock_fd, 
    struct sb_net_server_info *server_info,
    int ( *process_req )(str_t req, void * app_data),
    int ( *process_res )(str_t * res, void * app_data),
    void ( *init_app_data )(void *),
    int app_data_size
)
{
    int ret;
    struct sb_net_handler_ctx * ctx = sb_setup_thread_pipes(server_info, i); 
    if (ctx == NULL)
    {
        return SB_ERR_PIPE;
    }
    ctx->sock_fd = sock_fd;
    ctx->process_req = process_req;
    ctx->process_res = process_res;
    ctx->app_data = malloc(app_data_size);
    init_app_data(ctx->app_data);
    ret = pthread_create(&server_info->threads.thread_ids[i], NULL, sb_net_thread, ctx);
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
        make socket non-blocking
        */
        int flags = fcntl(sock_fd, F_GETFL, 0);
        if (ret == -1)
        {
            perror("fcntl, F_GETFL");
            return SB_ERR_FCNTL;
        }
        
        if (fcntl(sock_fd, F_SETFL, (flags | O_NONBLOCK)) != 0)
        {
            perror("fcntl, F_SETFL");
            return SB_ERR_FCNTL;
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
                        int ( *process_req )(str_t req, void * app_data),
                        int ( *process_res )(str_t * res, void * app_data),
                        void ( *init_app_data)(void * app_data),
                        const int app_data_size)
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
    // printf("sb_net_accept_conn - server: waiting for connections...\n");

    //min threads setup;
    pthread_t thread_ids[server_info->max_handlers];
    struct pollfd pipes_in[server_info->max_handlers];
    struct pollfd pipes_out[server_info->max_handlers];
    server_info->threads.pipes_in = pipes_in;
    server_info->threads.pipes_out = pipes_out;
    server_info->threads.thread_ids = thread_ids;

    for (int i=0; i<server_info->min_handlers; i++)
    {    
        if (sb_launch_thread(i, -1, server_info, process_req, process_res, init_app_data, app_data_size) != 0)
        {
            //TODO: actually handle errors
            printf("thread launch failed...exiting\n");
            exit(1);
        }
        server_info->len++;
    }
    
    int i;
    int ready;
    int pipes_ready_in = 0;
    int threads_ready = 0;
    int msg;

    // wait half a second for incoming connections
    struct pollfd sock;
    sock.fd = sock_fd;
    sock.events = POLLIN;
    int timeout = 500;
    

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
        i = threads_ready;
        ready = 0;
        pipes_ready_in = poll(pipes_in + threads_ready, server_info->len - threads_ready, 0);
        while (ready < pipes_ready_in)
        {
            if (pipes_in[i].revents & POLLIN){
                swap_thread_comm_structs(&server_info->threads, i, threads_ready+ready);
                ready++;
            }
            i++;
        } 

        //read from each ready pipe to see which thread sent the 
        // "I'm ready" message (SB_READY)
        i = threads_ready;
        ready = 0;
        while (ready < pipes_ready_in)
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
                swap_thread_comm_structs(&server_info->threads, i, threads_ready);
                threads_ready++;
                ready++;
            }
            i++;
        }

        if (threads_ready != pipes_ready_in)
        {
            //TODO: handle errors
            //if above condition is true, a thread sent a message other than SB_READY
        }

        while (threads_ready || server_info->len<server_info->max_handlers)
        {
            int incoming_conn = poll(&sock, 1, timeout);
            // printf("thread_count: %d, incoming_conn: %d\n", threads_ready, incoming_conn);

            if (incoming_conn > 0 && sock.revents & POLLIN)
            { 
                sin_size = sizeof(client_addr);
                conn_fd = accept(sock_fd, (struct sockaddr *)&client_addr, &sin_size);
                if (conn_fd == -1) 
                {
                    //TODO: handle errors
                    perror("accept");
                    continue;
                }


                inet_ntop(client_addr.ss_family, get_in_addr((struct sockaddr *)&client_addr), s, sizeof(s) );

                if (threads_ready==0) 
                {
                    if (server_info->len < server_info->max_handlers)
                    {
                        if (sb_launch_thread(server_info->len, conn_fd, server_info, process_req, process_res, init_app_data, app_data_size) != 0)
                        {
                            //TODO: actually handle errors
                            printf("thread launch failed...exiting\n");
                            exit(1);
                        }
                        server_info->len++;
                    }
                    else 
                    {
                        //pass connections to a busy thread
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
                    len = write(pipes_out[threads_ready-1].fd, &conn_fd, sizeof(int));
                    threads_ready--;
                    if (len == -1)
                    {
                        //TODO: handle errors
                        perror("write");
                        exit(1);
                    }
                }
            }
            else if (incoming_conn == -1)
            {
                //TODO: handle errors
                perror("poll");
                exit(1);
            }
            else 
            {
                /*
                    a timeout occurred on poll()
                */
                break;
            }
        }

        if (server_info->len > server_info->min_handlers)
        {
            //close the last open thread.
            int close_msg = SB_CLOSE; 
            len = write(pipes_out[server_info->len-1].fd, &close_msg, sizeof(int));
            if (len == -1)
            {
                //TODO: handle errors
                perror("write");
                exit(1);
            }
            close(pipes_out[server_info->len-1].fd);
            close(pipes_in[server_info->len-1].fd);

            server_info->len-=1;
        }

    }
}