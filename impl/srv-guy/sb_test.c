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

struct app_t {
	int request;
	int sent_count;
};

enum app_t_requests{
	APP_GET_DOUBLE,
	APP_GET_INT,
	APP_GET_10_DOUBLE,
	APP_GET_10_INT
};

static int test_mini_req_processor(
	const char * req, 
	const ssize_t req_len, 
	void * app_data)
{

	if (req[req_len-1] == '\n' && req[req_len-2] == '\n'){
		struct app_t * app = (struct app_t *) app_data;
		app->sent_count = 0;
		if (memcmp(req, "GET DOUBLE", 10) == 0){
			app->request = APP_GET_DOUBLE;
		} else if (memcmp(req, "GET INT", 7) == 0){
			app->request = APP_GET_INT;
		} else if (memcmp(req, "GET 10 INT", 10) == 0){
			app->request = APP_GET_10_INT;
		} else if (memcmp(req, "GET 10 DOUBLE", 13) == 0){
			app->request = APP_GET_10_DOUBLE;
		} else {
			return SB_APP_ERR;
		}
		return SB_APP_SEND;
	} else {
		return SB_APP_RECV_MORE;
	}
}

static int test_mini_res_processor(
	char * res, 
	ssize_t * res_len, 
	void * app_data
)
{
	struct app_t * app = (struct app_t *) app_data;
	int x = 2;
	double y = 3.1415;

	if (app->request == APP_GET_INT){
		memcpy(res, &x, sizeof(int));
		memcpy(res + sizeof(int), "\n\n", 2);
		*res_len = sizeof(int) + 2;
		return SB_APP_RECV;
	}
	
	if (app->request == APP_GET_DOUBLE){
		memcpy(res, &y, sizeof(double));
		memcpy(res + sizeof(double), "\n\n", 2);
		*res_len = sizeof(double) + 2;
		return SB_APP_RECV;
	}

	if (app->request == APP_GET_10_INT){
		memcpy(res, &x, sizeof(int));
		memcpy(res + sizeof(int), "\n\n", 2);
		*res_len = sizeof(int) + 2;
		app->sent_count++;

		if (app->sent_count==10){
			return SB_APP_RECV;
		} else {
			app->sent_count = 0;
			return SB_APP_SEND;
		}
	}

	if (app->request == APP_GET_10_DOUBLE){
		memcpy(res, &y, sizeof(double));
		memcpy(res + sizeof(double), "\n\n", 2);
		*res_len = sizeof(double) + 2;
		app->sent_count++;

		if (app->sent_count==10){
			return SB_APP_RECV;
		} else {
			app->sent_count = 0;
			return SB_APP_SEND;
		}
	}
}

void * test_mini_server(void * server_ctx){
	struct sb_net_server_info * srv_ctx = (struct sb_net_server_info *) server_ctx;
	sb_net_accept_conn(srv_ctx, test_mini_req_processor, test_mini_res_processor, 0);
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

	mu_assert(server_info->len==3, "server_info->len==3");

	//fourth connection
	int sockfd_4 = socket(AF_INET, SOCK_STREAM, 0);
	int buf4 = 4;
	mu_assert(connect(sockfd_4, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) >= 0, "sockfd_4 failed to connect");
	send(sockfd_4 , &buf4, sizeof(int) , 0 );
	sleep(1);

	mu_assert(server_info->len==4, "server_info->len==4");

	recv(sockfd_1, &buf1, sizeof(int), 0);
	recv(sockfd_2, &buf2, sizeof(int), 0);
	recv(sockfd_3, &buf3, sizeof(int), 0);
	recv(sockfd_4, &buf4, sizeof(int), 0);

	mu_assert(buf1 == 2, "buf1 == 2");
	mu_assert(buf2 == 4, "buf2 == 4");
	mu_assert(buf3 == 6, "buf3 == 6");
	mu_assert(buf4 == 8, "buf4 == 8");


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