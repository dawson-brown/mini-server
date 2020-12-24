#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "cb_tests.h"

#include "cb_main_argp.h"

int tests_run = 0;

static char * test_cb_verify_args() 
{
    int ret;

    struct cb_backup_ctx arguments = { 0 };
    arguments.method |= CB_GET;
    arguments.category = "music";
    arguments.path = "/path/to/file";
    ret = cb_main_argp_verify_args(&arguments);
    mu_assert("error, cb_verify_args: 1", ret == SUCCESS);

    memset(&arguments, 0, sizeof(struct cb_backup_ctx));
    arguments.method |= CB_GET;
    arguments.method |= CB_PUT;
    arguments.category = "music";
    arguments.path = "/path/to/file";
    ret = cb_main_argp_verify_args(&arguments);
    mu_assert("error, cb_verify_args: 2", ret == FAILURE);

    return 0;
}
 
static char * all_tests() 
{
    mu_run_test(test_cb_verify_args);
    return 0;
 }
 
int main(void) 
{
    char *result = all_tests();
    if (result != 0) {
        printf("%s\n", result);
    }
    else {
        printf("ALL TESTS PASSED\n");
    }
    printf("Tests run: %d\n", tests_run);

    return result != 0;
}