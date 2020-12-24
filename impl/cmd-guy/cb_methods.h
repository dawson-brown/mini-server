#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#ifndef CB_CTX
#include "cb_backup_ctx.h"
#define CB_CTX
#endif

enum cb_methods_returns
{
    SUCCESS,
    FAILURE
};

int cb_methods_handle_get(struct cb_backup_ctx * ctx);

int cb_methods_handle_put(struct cb_backup_ctx * ctx);

int cb_methods_handle_info(struct cb_backup_ctx * ctx);

int cb_methods_handle_config(struct cb_backup_ctx * ctx);

int cb_methods_handle_search(struct cb_backup_ctx * ctx);