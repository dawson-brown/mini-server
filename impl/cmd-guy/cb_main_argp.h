#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <argp.h>

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

/* Used by main to communicate with parse_opt. */
struct arguments
{               
  int method;
  char *category;
  char *path;
  int all_drives;
  int num_drives;
  char *drives[4];
  int ref;
};

int cb_verify_args(struct arguments *arguments);

error_t parse_opt (int key, char *arg, struct argp_state *state);