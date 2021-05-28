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

#define LIST_LEN_2 8
MU_TEST(test_swap_n_bytes_2_passes) {

	int list[LIST_LEN_2] = {1, 2, 1, 0, 0, 2, 1, 0};
	int i=0;
	int ready=5;
	int ready_count = 0;
	while (ready_count < ready)
	{
		if (list[i]){
			swap_n_bytes(&list[i], &list[ready_count], sizeof(int));
			ready_count++;
		}
		i++;
	}

	i=0;
	ready = ready_count;
	ready_count = 0;
	while (i<ready)
	{
		if (list[i] == 1){
			swap_n_bytes(&list[i], &list[ready_count], sizeof(int));
			ready_count++;
		}
		i++;
	}

	mu_check(list[0] == 1);
	mu_check(list[1] == 1);
	mu_check(list[2] == 1);
	mu_check(list[3] == 2);
	mu_check(list[4] == 2);
	mu_check(list[5] == 0);
	mu_check(list[6] == 0);
	mu_check(list[7] == 0);
}


static ssize_t test_mini_recv(int socket, void * buf, size_t n, int flags) 
{
	//an integer is sent to each thread, and the thread sleeps for that long
	ssize_t len = recv(socket, buf, sizeof(int), 0);
	sleep(1);
    return len;
}

static ssize_t test_mini_send(int socket, void * buf, size_t n, int flags) 
{
	printf("buf: %d\n", *(int *)buf);
	ssize_t len = send(socket, buf, n, flags);
    return len;
}

ssize_t test_mini_req_processor(char * req, ssize_t req_len, char * res, ssize_t res_len)
{
	memcpy(res, req, req_len);
	res_len = req_len;
    return res_len;
}

void * test_mini_server(void * server_ctx){
	struct sb_net_server_info * srv_ctx = (struct sb_net_server_info *) server_ctx;
	sb_net_accept_conn(srv_ctx, test_mini_recv, test_mini_send, test_mini_req_processor);
	return NULL;
}

#define TEST_PORT "8080"
#define TEST_ADDR "127.0.0.1"
MU_TEST(test_mini_server_instance){
	struct sb_net_server_info * server_info = sb_net_server_info_setup(
		TEST_PORT,
		AF_UNSPEC,
		AI_PASSIVE,
		SOCK_STREAM,
		2,
		4,
		3
	);

	//server address
	struct sockaddr_in serv_addr;
	serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(atoi(TEST_PORT));
	mu_assert(inet_pton(AF_INET, TEST_ADDR, &serv_addr.sin_addr)>=0, "Invalid address/ Address not supported");

	pthread_t server_thread;
	pthread_create(&server_thread, NULL, test_mini_server, server_info);


	while(server_info->len != 3);
	mu_assert(server_info->max_handlers == 4, "server_info->max_handlers == 4");
	mu_assert(server_info->min_handlers == 3, "server_info->min_handlers == 3");
	mu_assert(server_info->current_thread == 0, "server_info->current_thread == 0");

	//first connection
	int sockfd_1 = socket(AF_INET, SOCK_STREAM, 0);
	int buf1 = 1;
	mu_assert(connect(sockfd_1, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) >= 0, "sockfd_1 failed to connect");
	send(sockfd_1 , &buf1, sizeof(int) , 0 );

	//second connection
	int sockfd_2 = socket(AF_INET, SOCK_STREAM, 0);
	int buf2 = 2;
	mu_assert(connect(sockfd_2, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) >= 0, "sockfd_2 failed to connect");
	send(sockfd_2 , &buf2, sizeof(int) , 0 );

	//third connection
	int sockfd_3 = socket(AF_INET, SOCK_STREAM, 0);
	int buf3 = 3;
	mu_assert(connect(sockfd_3, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) >= 0, "sockfd_3 failed to connect");
	send(sockfd_3 , &buf3, sizeof(int) , 0 );

	//fourth connection
	int sockfd_4 = socket(AF_INET, SOCK_STREAM, 0);
	int buf4 = 4;
	mu_assert(connect(sockfd_4, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) >= 0, "sockfd_4 failed to connect");
	send(sockfd_4 , &buf4, sizeof(int) , 0 );

	recv(sockfd_1, &buf1, sizeof(int), 0);
	recv(sockfd_2, &buf2, sizeof(int), 0);
	recv(sockfd_3, &buf3, sizeof(int), 0);
	recv(sockfd_4, &buf4, sizeof(int), 0);

	mu_assert(buf1 == 1, "buf1 == 1");
	mu_assert(buf2 == 2, "buf2 == 2");
	mu_assert(buf3 == 3, "buf3 == 3");
	mu_assert(buf4 == 4, "buf4 == 4");

	free(server_info);
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
	MU_RUN_TEST(test_swap_n_bytes_2_passes);
	MU_RUN_TEST(test_get_in_addr);
	MU_RUN_TEST(test_safe_close);
	MU_RUN_TEST(test_mini_server_instance);
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