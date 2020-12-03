#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "cb_tests.h"

#include "cb_main_argp.h"

int tests_run = 0;

static char * test_foo() 
{
    // mu_assert("error, foo != 7", foo == 7);
    return 0;
}
 
static char * all_tests() 
{
    mu_run_test(test_foo);
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