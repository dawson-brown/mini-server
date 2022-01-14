#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct thread_tracker 
{
    pthread_t * thread_ids;
    struct pollfd * pipes_out;
    struct pollfd * pipes_in;
    int len;
};
