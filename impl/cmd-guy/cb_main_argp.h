#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <argp.h>

#ifndef CB_CTX
#include "cb_backup_ctx.h"
#define CB_CTX
#endif

#define CB_MAX_DRIVES 4

enum cb_main_ret 
{
    SUCCESS,
    FAILURE,
};

extern const char *argp_program_version;
extern const char *argp_program_bug_address;

/* Program documentation. */
extern const char doc[];

/* A description of the arguments we accept. */
extern const char args_doc[];

enum cb_options 
{
    CB_OPTION_GRP = 0x00,
    CB_CATEGORY = 'c',
    CB_PATH = 'p',
    CB_DRIVES = 'd',
    CB_REF = 'r',

    CB_METHOD_GRP = 0x80,
    CB_OPT_GET,
    CB_OPT_PUT,
    CB_OPT_INFO,
    CB_OPT_CONFIG,
    CB_OPT_SEARCH
};

enum cb_method_flags
{
    CB_GET = 0x2,
    CB_PUT = 0x4,
    CB_INFO = 0x8,
    CB_CONFIG = 0x10,
    CB_SEARCH = 0x20
};

extern const struct argp_option options[];

int cb_main_argp_verify_args(struct cb_backup_ctx *arguments);

error_t cb_main_argp_parse_opt (int key, char *arg, struct argp_state *state);