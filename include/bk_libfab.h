#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <stdio.h>
#include <getopt.h>

#define BK_OUT(_str, ...) printf(_str"\n", ##__VA_ARGS__);
#define BK_OUT_ERR(_str, ...) fprintf(stderr, "BK_ERR file: %s line %d msg: "_str"\n", __FILE__, __LINE__, ##__VA_ARGS__);

#define BK_OOB_PORT 8228

typedef enum {
	BK_OK = 0,
	BK_ERR = 1,
} bk_status_t;

typedef struct bk_opt_t {
	int n;
	char* port;
	char* srv_addr;
}bk_opt_t;

typedef struct bk_server_t {
	int* clients;
} bk_server_t;

typedef struct bk_bmark_t {
	bk_opt_t bk_opts;
	struct fi_info* info;
	struct fi_info* hints;
	bk_server_t* srv_dat;
	int srv_soc;
	int rank;
}bk_bmark_t;


void bk_init_opts(bk_opt_t* bk_opts);

bk_status_t bk_parse_args(int argc, char* argv[], bk_opt_t* bk_opt);

void bk_print_info(bk_bmark_t* bk_bmark_data);

bk_status_t bk_oob_server_setup(bk_bmark_t* bk_dat, int bind_sock);
bk_status_t bk_oob_init(bk_bmark_t* bk_dat);
bk_status_t bk_oob_send(int socket, void* buf, size_t len);
bk_status_t bk_oob_recv(int socket, void* buf, size_t len);
bk_status_t bk_oob_init_ranks(bk_bmark_t* bk_dat);

bk_status_t bk_init_fabric(bk_bmark_t* bk_bmark_data);
void bk_cleanup(bk_bmark_t* bk_bmark_data);