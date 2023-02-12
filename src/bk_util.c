#include <rdma/fabric.h>
#include <rdma/fi_cm.h>
#include "bk_libfab.h"

void bk_cleanup(bk_bmark_t* bk_dat) {
	if(bk_dat->srv_dat){
		for(int i = 0; i<bk_dat->bk_opts.n-1; i++){
			close(bk_dat->srv_dat->clients[i]);
		}
		free(bk_dat->srv_dat->clients);
		free(bk_dat->srv_dat);
	}else{
		close(bk_dat->srv_soc);
	}
	fi_freeinfo(bk_dat->info);
	fi_freeinfo(bk_dat->hints);
}

void bk_print_info(bk_bmark_t* bk_bmark_data){
	char hname[128];
	gethostname(hname, 128);
	BK_OUT("### bk_libfab_coll pid: %d nnodes: %d, svr: %s:%s, running on %s",
			getpid(),
			bk_bmark_data->bk_opts.n,
			bk_bmark_data->bk_opts.srv_addr,
			bk_bmark_data->bk_opts.port,
			hname);
	fflush(stdout);
}

void bk_init_opts(bk_opt_t* bk_opts) {
	bk_opts->port = "9228";
	bk_opts->srv_addr = NULL;
	bk_opts->n = -1;
}

static struct option bk_arg_options[] = {
	{"port", required_argument, NULL, 'p'},
	{"server", required_argument, NULL, 's'},
	{"nnodes", required_argument, NULL, 'n'},
};

char * bk_opt_str="p:s:n:";

bk_status_t bk_parse_args(int argc, char* argv[], bk_opt_t* bk_opt) {
	while (1) {
		int c = getopt_long(argc, argv, bk_opt_str, bk_arg_options, NULL);
		if (-1 == c)break;
		switch (c) {
		case 'n':
			bk_opt->n = atoi(optarg);
			break;
		case 'p':
			bk_opt->port = optarg;
			break;
		case 's':
			bk_opt->srv_addr = optarg;
			break;
		case '?':
			return BK_ERR;
			break;
		default:
			break;
		}
	}

	return BK_OK;
}

bk_status_t bk_oob_server_setup(bk_bmark_t* bk_dat, int bind_sock) {
	int ret;

	bk_dat->srv_dat = malloc(sizeof(*bk_dat->srv_dat));
	bk_dat->srv_dat->clients = calloc(bk_dat->bk_opts.n - 1, sizeof(int));

	ret = listen(bind_sock, bk_dat->bk_opts.n);
	if (ret) {
		BK_OUT_ERR("server listen error");
		return BK_ERR;
	}

	for (int i = 0; i < bk_dat->bk_opts.n - 1; i++) {
		int nsoc = accept(bind_sock, NULL, NULL);
		if (0 > nsoc) {
			BK_OUT_ERR("server sock accept error");
			return BK_ERR;
		}
		bk_dat->srv_dat->clients[i] = nsoc;
	}

	return BK_OK;
}

bk_status_t bk_oob_init(bk_bmark_t* bk_dat) {
	struct addrinfo* res;
	int ret = BK_OK;
	int sock;
	struct sockaddr_in* sin;
	struct sockaddr_in6* sin6;

	ret = getaddrinfo(bk_dat->bk_opts.srv_addr, NULL, NULL, &res);
	if (ret) {
		BK_OUT_ERR("getaddrinfo failed: %s (%d)",
			bk_dat->bk_opts.srv_addr, ret
		);
		return BK_ERR;
	}

	switch (res->ai_addr->sa_family) {
	case AF_INET:
		sin = (struct sockaddr_in*)res->ai_addr;
		sin->sin_port = BK_OOB_PORT;
		break;
	case AF_INET6:
		sin6 = (struct sockaddr_in6*)res->ai_addr;
		sin6->sin6_port = BK_OOB_PORT;
		break;
	default:
		BK_OUT_ERR("bad server family");
		return BK_ERR;
	}

	sock = socket(res->ai_family, SOCK_STREAM, 0);
	if (sock < 0) {
		BK_OUT_ERR("Failed to create socket");
		return BK_ERR;
	}

	ret = bind(sock, res->ai_addr, res->ai_addrlen);
	if (0 == ret) {
		ret = bk_oob_server_setup(bk_dat, sock);
		close(sock);
		if (ret) {
			BK_OUT_ERR("server setup error");
			return BK_ERR;
		}
	}
	else {
		ret = connect(sock, res->ai_addr, res->ai_addrlen);
		if (ret) {
			BK_OUT_ERR("oob conn error (%s);%s",
				strerror(errno), res->ai_canonname
			);
			return BK_ERR;
		}
		bk_dat->srv_soc = sock;
	}

	freeaddrinfo(res);

	return BK_OK;
}

bk_status_t bk_oob_send(int socket, void* buf, size_t len) {
	size_t msent = 0, ret;
	uint8_t* sptr = buf;
	do {
		ret = send(socket, sptr + msent, len, 0);
		if (0 > msent) {
			BK_OUT_ERR("sock send error");
			return BK_ERR;
		}
		msent += ret;
	} while (msent < len);

	return BK_OK;
}

bk_status_t bk_oob_recv(int socket, void* buf, size_t len) {
	size_t mrecv = 0, ret;
	uint8_t* sptr = buf;

	do {
		ret = recv(socket, sptr + mrecv, len, 0);
		if (0 > ret) {
			BK_OUT_ERR("sock recv error");
			return BK_ERR;
		}
		mrecv += ret;
	} while (mrecv < len);

	return BK_OK;
}

bk_status_t bk_oob_init_ranks(bk_bmark_t* bk_dat) {
	if (bk_dat->srv_dat) {
		bk_dat->rank = 0;
		for (int i = 0; i < bk_dat->bk_opts.n - 1; i++) {
			int srank = i + 1;
			if (bk_oob_send(bk_dat->srv_dat->clients[i], &srank, sizeof(srank))) {
				BK_OUT_ERR("rank send error");
				return BK_ERR;
			}
		}

	}
	else {
		int rrank;
		if (bk_oob_recv(bk_dat->srv_soc, &rrank, sizeof(rrank))) {
			BK_OUT_ERR("rank recv error");
			return BK_ERR;
		}
		bk_dat->rank = rrank;
	}
	
	return BK_OK;
}

bk_status_t bk_init_fabric(bk_bmark_t* bk_bmark_data) {
	bk_opt_t* bk_opt = &bk_bmark_data->bk_opts;
	struct fi_info* info, * hints = bk_bmark_data->hints;

	fi_getinfo(FI_VERSION(1, 17), bk_opt->srv_addr, bk_opt->port, 0, hints, &info);
	bk_bmark_data->info = info;

	BK_OUT("Selected provider: %s (%s)", info->fabric_attr->prov_name, info->domain_attr->name);

	return BK_OK;
}