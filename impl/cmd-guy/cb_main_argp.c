#include "cb_main_argp.h"

const char *argp_program_version = "mini-backup 1.0";
const char *argp_program_bug_address = "";

/* Program documentation. */
const char doc[] = "Mini-backup command line tool to talk to mini-backup server.";

/* A description of the arguments we accept. */
const char args_doc[] = "";

const struct argp_option options[] = {
    {"get",  CB_GET, 0,  0,  "Get a file from storage.", CB_METHOD_GRP },
    {"put",   CB_PUT, 0, 0, "Put a file to storage.", CB_METHOD_GRP },
    {"search",  CB_SEARCH, 0,  0,  "Search the storage device.", CB_METHOD_GRP },
    {"info",   CB_INFO, 0, 0, "Get info about the storage device.", CB_METHOD_GRP },
    {"config",   CB_CONFIG, 0, 0, "Configure the storage device.", CB_METHOD_GRP },

    {"category",   CB_CATEGORY, "category", 0, "Select a category.", CB_OPTION_GRP },
    {"path",   CB_PATH, "file path", 0, "Provide a file path.", CB_OPTION_GRP },
    {"drives",   CB_DRIVES, "drives", OPTION_ARG_OPTIONAL, "Optionally specify drives, or indicate to --info you want to see info about the all drives.", CB_OPTION_GRP },
    {"refactor",   CB_REF, 0, 0, "Configure the storage device.", CB_OPTION_GRP },
    { 0 }
};

int cb_verify_args(struct arguments *arguments)
{
    if ( (arguments->method | CB_GET) == CB_GET )
    {
        return FAILURE;
    }

    if ( (arguments->method | CB_PUT) == CB_PUT )
    {
        return FAILURE;
    }

    if ( (arguments->method | CB_INFO) == CB_INFO )
    {
        return FAILURE;
    }

    if ( (arguments->method | CB_CONFIG) == CB_CONFIG )
    {
        return FAILURE;
    }

    if ( (arguments->method | CB_SEARCH) == CB_SEARCH )
    {
        return FAILURE;
    }

    return FAILURE;
}


/* Parse a single option. */
error_t parse_opt (int key, char *arg, struct argp_state *state)
{
  /* Get the input argument from argp_parse, which we
     know is a pointer to our arguments structure. */
  struct arguments *arguments = state->input;

  switch (key)
    {
    case CB_CATEGORY:
        arguments->category = arg;
        break;
    case CB_PATH:
        arguments->path = arg;
        break;
    case CB_DRIVES:
        if (arg != NULL)
        {   
            if (arguments->num_drives < CB_MAX_DRIVES)
            {
                arguments->drives[arguments->num_drives++] = arg;
            }
            else 
            {
                argp_usage (state);
            }
        }
        else 
        {
            arguments->all_drives = CB_DRIVES;
        }
        break;
    case CB_REF:
        break;
    case CB_OPT_GET:
        arguments->method |= CB_GET;
        break;
    case CB_OPT_PUT:
        arguments->method |= CB_PUT;
        break;
    case CB_OPT_INFO:
        arguments->method |= CB_INFO;
        break;
    case CB_OPT_CONFIG:
        arguments->method |= CB_CONFIG;
        break;
    case CB_OPT_SEARCH:
        arguments->method |= CB_SEARCH;
        break;
    case ARGP_KEY_END:
        if (cb_verify_args(arguments) != SUCCESS)
        {
            argp_usage (state);
        }
        break;

    default:
        return ARGP_ERR_UNKNOWN;
    }
  return 0;
}