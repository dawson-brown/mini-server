#include "sb_net.c"

#include "minunit.h"

void test_setup(void) {
    /* TODO */
}

void test_teardown(void) {
	/* TODO */
}

MU_TEST(test_sb_net_socket_setup) {
    /* TODO */
	mu_check(0 == 0);
}

#define LIST_LEN 6
MU_TEST(test_swap_n_bytes) {

	int list[LIST_LEN] = {0, 1, 1, 0, 1 ,0};
	int i=0;
	int ready=3;
	int ready_count = 0;
	while (ready_count < ready)
	{
		if (list[i]==0){
			swap_n_bytes(&list[i], &list[ready_count], sizeof(int));
			ready_count++;
		}
		i++;
	}

	mu_check(list[0] == 0);
	mu_check(list[1] == 0);
	mu_check(list[2] == 0);
	mu_check(list[3] == 1);
	mu_check(list[4] == 1);
	mu_check(list[5] == 1);
}

MU_TEST(test_get_in_addr) {
    /* TODO */
	mu_check(0 == 0);
}

MU_TEST(test_safe_close) {
    /* TODO */
	mu_check(0 == 0);
}

MU_TEST_SUITE(sb_net_test_suite) {
	MU_SUITE_CONFIGURE(&test_setup, &test_teardown);

	MU_RUN_TEST(test_sb_net_socket_setup);
	MU_RUN_TEST(test_swap_n_bytes);
	MU_RUN_TEST(test_get_in_addr);
	MU_RUN_TEST(test_safe_close);
}

int main(int argc, char *argv[]) {
	MU_RUN_SUITE(sb_net_test_suite);
	MU_REPORT();
	return MU_EXIT_CODE;
}

/******************************************  MINUNIT examples 
MU_TEST(test_check_fail) {
	mu_check(foo != 7);
}

MU_TEST(test_assert) {
	mu_assert(foo == 7, "foo should be 7");
}

MU_TEST(test_assert_fail) {
	mu_assert(foo != 7, "foo should be <> 7");
}

MU_TEST(test_assert_int_eq) {
	mu_assert_int_eq(4, bar);
}

MU_TEST(test_assert_int_eq_fail) {
	mu_assert_int_eq(5, bar);
}

MU_TEST(test_assert_double_eq) {
	mu_assert_double_eq(0.1, dbar);
}

MU_TEST(test_assert_double_eq_fail) {
	mu_assert_double_eq(0.2, dbar);
}

MU_TEST(test_fail) {
	mu_fail("Fail now!");
}

MU_TEST(test_string_eq){
	mu_assert_string_eq("Thisstring", foostring);
}

MU_TEST(test_string_eq_fail){
	mu_assert_string_eq("Thatstring", foostring);
}
 ******************************************************************/