#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "cb_main_argp.h"


static struct argp argp = { options, parse_opt, args_doc, doc };

int main(int argc, char ** argv)
{
    struct arguments arguments = { 0 };
    argp_parse (&argp, argc, argv, 0, 0, &arguments);

    printf("Goodbye.\n");
    return 0;
}