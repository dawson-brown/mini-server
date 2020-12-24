#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "cb_main_argp.h"
#include "cb_methods.h"


static struct argp argp = { options, cb_main_argp_parse_opt, args_doc, doc };

int main(int argc, char ** argv)
{
    struct cb_backup_ctx arguments = { 0 };
    argp_parse (&argp, argc, argv, 0, 0, &arguments);

    if ( arguments.method == CB_GET )
    {
        return cb_methods_handle_get(&arguments);
    }

    else if ( arguments.method == CB_PUT )
    {
        return cb_methods_handle_put(&arguments);
    }

    else if ( arguments.method == CB_INFO )
    {   
        return cb_methods_handle_info(&arguments);
    }

    else if ( arguments.method == CB_CONFIG )
    {
        return cb_methods_handle_config(&arguments);
    }

    else if ( arguments.method == CB_SEARCH )
    {
        return cb_methods_handle_search(&arguments);
    }

    else 
    {
        //this will never happen if cb_main_argp_verify_args is working correctly
        return FAILURE;
    }

    return SUCCESS;
}