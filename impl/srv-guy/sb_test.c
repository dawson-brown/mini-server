#include "minunit.h"

#include "sb_net.c"

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

struct app_t {
	int request;
	int sent_count;
};

enum app_t_requests{
	APP_GET_DOUBLE,
	APP_GET_INT,
	APP_GET_3_DOUBLE,
	APP_GET_3_INT,
	APP_ERR
};

static int test_mini_req_processor(
	str_t * req, 
	void * app_data)
{
	if (req->buffer[req->data_len-1] == '\n' && req->buffer[req->data_len-2] == '\n'){
		struct app_t * app = (struct app_t *) app_data;
		app->sent_count = 0;
		if (memcmp(req->buffer, "GET DOUBLE", 10) == 0){
			app->request = APP_GET_DOUBLE;
		} else if (memcmp(req->buffer, "GET INT", 7) == 0){
			app->request = APP_GET_INT;
		} else if (memcmp(req->buffer, "GET 3 INT", 9) == 0){
			app->request = APP_GET_3_INT;
		} else if (memcmp(req->buffer, "GET 3 DOUBLE", 12) == 0){
			app->request = APP_GET_3_DOUBLE;
		} else {
			app->request = APP_ERR;
			return SB_APP_ERR;
		}
		return SB_APP_SEND;
	} else {
		return SB_APP_RECV_MORE;
	}
}

static int test_mini_res_processor(
	str_t * res, 
	void * app_data
)
{
	struct app_t * app = (struct app_t *) app_data;
	int x = 2;
	double y = 3.1415;

	if (app->request == APP_GET_INT){
		memcpy(res->buffer, &x, sizeof(int));
		res->data_len = sizeof(int);
		return SB_APP_CONN_DONE;
	}
	
	if (app->request == APP_GET_DOUBLE){
		memcpy(res->buffer, &y, sizeof(double));
		res->data_len = sizeof(double);
		return SB_APP_CONN_DONE;
	}

	if (app->request == APP_GET_3_INT){
		memcpy(res->buffer, &x, sizeof(int));
		res->data_len = sizeof(int);
		app->sent_count++;

		if (app->sent_count==3){
			return SB_APP_CONN_DONE;
		} else {
			return SB_APP_SEND;
		}
	}

	if (app->request == APP_GET_3_DOUBLE){
		memcpy(res->buffer, &y, sizeof(double));
		res->data_len = sizeof(double);
		app->sent_count++;

		if (app->sent_count==3){
			return SB_APP_CONN_DONE;
		} else {
			return SB_APP_SEND;
		}


	}

	if (app->request == APP_ERR){
		int e = -1;
		memcpy(res->buffer, &e, sizeof(int));
		res->data_len = sizeof(int);
		return SB_APP_CONN_DONE;
	}

	return SB_APP_FATAL_ERR;
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
		4,
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

	sleep(2);



	mu_assert(server_info->max_handlers == 4, "server_info->max_handlers == 4");
	mu_assert(server_info->min_handlers == 3, "server_info->min_handlers == 3");
	mu_assert(server_info->current_thread == 0, "server_info->current_thread == 0");

	double recv1=0;
	int recv2=0;
	int recv3=0;
	double recv4=0;
	int recv5=0;
	
	//first sock
	int sockfd_1 = socket(AF_INET, SOCK_STREAM, 0);
	char buf1[13] = "GET DOUBLE\n\n";

	//second sock
	int sockfd_2 = socket(AF_INET, SOCK_STREAM, 0);
	char buf2[10] = "GET INT\n\n";

	//third sock
	int sockfd_3 = socket(AF_INET, SOCK_STREAM, 0);
	char buf3[12] = "GET 3 INT\n\n";

	//fourth sock
	int sockfd_4 = socket(AF_INET, SOCK_STREAM, 0);
	char buf4[15] = "GET 3 DOUBLE\n\n";

	int sockfd_5 = socket(AF_INET, SOCK_STREAM, 0);
	char buf5[19] = "This is an error\n\n";


	connect(sockfd_1, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
	connect(sockfd_2, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
	connect(sockfd_3, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
	connect(sockfd_4, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
	connect(sockfd_5, (struct sockaddr *)&serv_addr, sizeof(serv_addr));

	send(sockfd_1, buf1, 12 , 0 );
	send(sockfd_2, buf2, 9 , 0 );
	send(sockfd_3, buf3, 11 , 0 );
	send(sockfd_4, buf4, 14 , 0 );
	send(sockfd_5, buf5, 18 , 0 );

	recv(sockfd_1, &recv1, sizeof(double), 0);
	mu_assert(recv1 == 3.1415, "recv1 == 3.1415");

	recv(sockfd_2, &recv2, sizeof(int), 0);
	mu_assert(recv2 == 2, "recv2 == 2");

	recv(sockfd_3, &recv3, sizeof(int), 0);
	mu_assert(recv3 == 2, "recv3 == 2");
	recv3=0;
	recv(sockfd_3, &recv3, sizeof(int), 0);
	mu_assert(recv3 == 2, "recv3 == 2");
	recv3=0;
	recv(sockfd_3, &recv3, sizeof(int), 0);
	mu_assert(recv3 == 2, "recv3 == 2");

	recv(sockfd_4, &recv4, sizeof(double), 0);
	mu_assert(recv4 == 3.1415, "recv4 == 3.1415");
	recv4=0;
	recv(sockfd_4, &recv4, sizeof(double), 0);
	mu_assert(recv4 == 3.1415, "recv4 == 3.1415");
	recv4=0;
	recv(sockfd_4, &recv4, sizeof(double), 0);
	mu_assert(recv4 == 3.1415, "recv4 == 3.1415");
	recv4=0;

	recv(sockfd_5, &recv5, sizeof(int), 0);
	mu_assert(recv5 == -1, "recv5 == -1");

	sleep(1);
	mu_assert(server_info->len==3, "server_info->len==3");

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