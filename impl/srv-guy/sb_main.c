#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "mb_main.h"

#define MB_GET "get"
#define MB_PUT "put"
#define MB_INFO "info"
#define MB_CONFIG "config"


int main(int argc, char ** argv)
{   
    if (strcmp("help", argv[1]) == 0 || argc < 2)
    {
        printf("Usage\n");
    }

    if (strcmp(MB_GET, argv[1]) == 0)
    {

    }
    else if (strcmp(MB_PUT, argv[1]) == 0)
    {

    } 
    else if (strcmp(MB_INFO, argv[1]) == 0)
    {

    } 
    else if (strcmp(MB_CONFIG, argv[1]) == 0)
    {

    }
    else 
    {
        
    }

    printf("Goodbye.\n");
    return 0;

}