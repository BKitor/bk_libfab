#ifndef BK_LIBFAB_H
#define BK_LIBFAB_H

#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <stdio.h>
#include <getopt.h>

#define BK_OOB_PORT 8228

#define BK_OUT_OOB(_str, ...) if(bk_bmark.bk_opts.verbosity >= BK_V_OOB){printf("OOB [%d: %s::%s::%d] "_str"\n",getpid(),__FILE__,__FUNCTION__,__LINE__, ##__VA_ARGS__);fflush(stdout);}
#define BK_OUT_DEBUG(_str, ...) if(bk_bmark.bk_opts.verbosity >= BK_V_DEBUG){printf("DEBUG [%d: %s::%s::%d] "_str"\n",getpid(),__FILE__,__FUNCTION__,__LINE__, ##__VA_ARGS__);fflush(stdout);}
#define BK_OUT_INFO(_str, ...) if(bk_bmark.bk_opts.verbosity >= BK_V_INFO){printf("INFO [%d: %s::%s::%d] "_str"\n",getpid(),__FILE__,__FUNCTION__,__LINE__, ##__VA_ARGS__);fflush(stdout);}

#define BK_OUT_ERROR(_str, ...) fprintf(stderr,"ERROR [%d: %s::%s::%d] "_str"\n",getpid(),__FILE__,__FUNCTION__,__LINE__, ##__VA_ARGS__);fflush(stderr)

typedef enum {
	BK_OK = 0,
	BK_ERR = 1,
} bk_status_t;

typedef enum {
	BK_V_OOB = 9,
	BK_V_DEBUG = 6,
	BK_V_INFO = 4,
	BK_V_ERROR = 1,
} bk_verbosity_t;

typedef struct bk_opt_t {
	int n;
	char* oob_port;
	char* srv_addr;
	int verbosity;
	size_t ssize;
	size_t esize;
	int32_t iters;
	int32_t warmups;
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

	struct fid_fabric* fabric;
	struct fid_domain* domain;
	struct fid_ep* ep;
	struct fid_cq* cq;
	struct fid_av* av;
	fi_addr_t* peeraddrs;
}bk_bmark_t;


extern bk_bmark_t bk_bmark;

void bk_init_bmark(bk_bmark_t* bk_bmark);

bk_status_t bk_parse_args(int argc, char* argv[], bk_opt_t* bk_opts);

void bk_print_info();

bk_status_t bk_oob_server_setup(int bind_sock);
bk_status_t bk_oob_init();
bk_status_t bk_oob_send(int socket, void* buf, size_t len);
bk_status_t bk_oob_recv(int socket, void* buf, size_t len);
bk_status_t bk_oob_init_ranks();
bk_status_t bk_oob_barrier();

bk_status_t bk_init_fabric();
void bk_cleanup();

bk_status_t bk_wait_cq();

#endif /* BK_LIBFAB_H */