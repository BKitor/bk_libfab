#include <rdma/fabric.h>
#include <rdma/fi_cm.h>

#include "bk_libfab.h"

bk_bmark_t bk_bmark = { 0 };

void bk_cleanup() {
	BK_OUT_INFO("Running Cleanup");
	fi_close(&bk_bmark.ep->fid);
	fi_close(&bk_bmark.av->fid);
	fi_close(&bk_bmark.cq->fid);
	fi_close(&bk_bmark.domain->fid);
	fi_close(&bk_bmark.fabric->fid);

	if (bk_bmark.sbuf) free(bk_bmark.sbuf);
	if (bk_bmark.rbuf) free(bk_bmark.rbuf);

	if (bk_bmark.srv_dat) {
		for (int i = 0; i < bk_bmark.bk_opts.n - 1; i++) {
			shutdown(bk_bmark.srv_dat->clients[i], SHUT_RD);
			close(bk_bmark.srv_dat->clients[i]);
		}
		free(bk_bmark.srv_dat->clients);
		free(bk_bmark.srv_dat);
	}
	else {
		shutdown(bk_bmark.srv_soc, SHUT_RDWR);
		close(bk_bmark.srv_soc);
	}
	fi_freeinfo(bk_bmark.info);
	// fi_freeinfo(bk_bmark.hints);
}

void bk_print_info(bk_bm_def_t* bm_lst) {
	char hname[128];
	gethostname(hname, 128);
	BK_OUT_INFO("### bk_libfab_coll pid: %d nnodes: %d, svr: %s:%s, running on %s, msizes(%ld->%ld), iters: %d, warmups: %d",
		getpid(),
		bk_bmark.bk_opts.n,
		bk_bmark.bk_opts.srv_addr,
		bk_bmark.bk_opts.oob_port,
		hname,
		bk_bmark.bk_opts.ssize,
		bk_bmark.bk_opts.esize,
		bk_bmark.bk_opts.iters,
		bk_bmark.bk_opts.warmups
	);
	fflush(stdout);
	char* bm_out_str = calloc(1 << 10, sizeof(char));
	strcat(bm_out_str, "### Benchmarks: ");

	bm_lst = bm_lst->next;
	strcat(bm_out_str, bm_lst->name);

	while (bm_lst->next) {
		bm_lst = bm_lst->next;

		strcat(bm_out_str, ", ");
		strcat(bm_out_str, bm_lst->name);
	}
	BK_OUT_INFO("%s", bm_out_str)

	fflush(stdout);
	free(bm_out_str);
}

void bk_bmark_construct(bk_bmark_t* bm) {
	bk_opt_t* bk_opts = &bm->bk_opts;
	bk_opts->verbosity = 0;
	bk_opts->oob_port = "9228";
	bk_opts->srv_addr = NULL;
	bk_opts->bm_str = "barrier,tag_ring";
	bk_opts->n = -1;
	bk_opts->ssize = (1 >> 5);
	bk_opts->esize = (1 >> 10);
	bk_opts->iters = 200;
	bk_opts->warmups = 20;
}

int bk_parse_msizes(bk_opt_t* bk_opt, char* mstr) {
	char* tmpstr = alloca(strlen(mstr) + 1);
	strcpy(tmpstr, mstr);
	char* fcol = strchr(tmpstr, ':');
	char* lcol = strrchr(tmpstr, ':');

	if (!fcol || fcol != lcol) {
		BK_OUT_ERROR("bad msize str (%s)", tmpstr);
		return BK_ERR;
	}
	fcol[0] = '\0';
	fcol += 1;

	bk_opt->ssize = atoll(tmpstr);
	bk_opt->esize = atoll(fcol);

	if (bk_opt->ssize >= bk_opt->esize) {
		BK_OUT_ERROR("bad message sizes: start: %ld, end: %ld", bk_opt->ssize, bk_opt->esize);
		return BK_ERR;
	}

	return BK_OK;
}

static struct option bk_arg_options[] = {
	{"benchmaks", 	required_argument, NULL, 'b'},
	{"itterations",	required_argument, NULL, 'i'},
	{"msizes", 		required_argument, NULL, 'm'},
	{"nnodes", 		required_argument, NULL, 'n'},
	{"port", 		required_argument, NULL, 'p'},
	{"server", 		required_argument, NULL, 's'},
	{"verbosity", 	required_argument, NULL, 'v'},
	{"warmups", 	required_argument, NULL, 'w'},
};

char* bk_opt_str = "b:i:m:n:p:s:v:w:";

bk_status_t bk_parse_args(int argc, char* argv[], bk_opt_t* bk_opt) {
	int ret = BK_OK;
	while (1) {
		int c = getopt_long(argc, argv, bk_opt_str, bk_arg_options, NULL);
		if (-1 == c)break;
		switch (c) {
		case 'b':
			bk_opt->bm_str = optarg;
			break;
		case 'i':
			bk_opt->iters = atoi(optarg);
			break;
		case 'm':
			if ((ret = bk_parse_msizes(bk_opt, optarg))) {
				return ret;
			}
			break;
		case 'n':
			bk_opt->n = atoi(optarg);
			break;
		case 'p':
			bk_opt->oob_port = optarg;
			break;
		case 's':
			bk_opt->srv_addr = optarg;
			break;
		case 'v':
			bk_opt->verbosity = atoi(optarg);
			break;
		case 'w':
			bk_opt->warmups = atoi(optarg);
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

bk_status_t bk_parse_def_csl(bk_bm_def_t* def_lst) {
	char* tok = NULL, * strtmp = NULL;
	const char comma[2] = ",";

	if (!bk_bmark.bk_opts.bm_str) {
		BK_OUT_ERROR("benchmark comma-seperated-list can't be NULL");
		return BK_ERR;
	}

	strtmp = alloca(strlen(bk_bmark.bk_opts.bm_str));
	strcpy(strtmp, bk_bmark.bk_opts.bm_str);

	tok = strtok(strtmp, comma);

	while (NULL != tok) {
		int m, m_flag = 0;
		BK_OUT_DEBUG("strtok found token %s", tok);
		for (int i = 0; i < bk_iter_def_arr_len; i++) {
			m = strcmp(bk_iter_def_arr[i].name, tok);
			if (0 == m) {
				BK_OUT_DEBUG("matched %s/%s", tok, bk_iter_def_arr[i].name);
				m_flag = 1;
				def_lst->next = &bk_iter_def_arr[i];
				def_lst = def_lst->next;
			}
		}
		if (!m_flag) {
			BK_OUT_ERROR("Could not find benchmark \"%s\"", tok);
			return BK_ERR;
		}
		tok = strtok(NULL, comma);
	}


	return BK_OK;
}

bk_status_t bk_oob_server_setup(int bind_sock) {
	int ret;

	bk_bmark.srv_dat = malloc(sizeof(*bk_bmark.srv_dat));
	bk_bmark.srv_dat->clients = calloc(bk_bmark.bk_opts.n - 1, sizeof(int));

	ret = listen(bind_sock, bk_bmark.bk_opts.n);
	if (ret) {
		BK_OUT_ERROR("server listen error");
		return BK_ERR;
	}

	for (int i = 0; i < bk_bmark.bk_opts.n - 1; i++) {
		int nsoc = accept(bind_sock, NULL, NULL);
		if (0 > nsoc) {
			BK_OUT_ERROR("server sock accept error");
			return BK_ERR;
		}
		bk_bmark.srv_dat->clients[i] = nsoc;
	}

	return BK_OK;
}

bk_status_t bk_oob_init() {
	struct addrinfo* res;
	int ret = BK_OK;
	int sock;
	struct sockaddr_in* sin;
	struct sockaddr_in6* sin6;

	ret = getaddrinfo(bk_bmark.bk_opts.srv_addr, NULL, NULL, &res);
	if (ret) {
		BK_OUT_ERROR("getaddrinfo failed: %s (%d)",
			bk_bmark.bk_opts.srv_addr, ret
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
		BK_OUT_ERROR("bad server family");
		return BK_ERR;
	}

	sock = socket(res->ai_family, SOCK_STREAM, 0);
	if (sock < 0) {
		BK_OUT_ERROR("Failed to create socket");
		return BK_ERR;
	}

	ret = bind(sock, res->ai_addr, res->ai_addrlen);
	if (0 == ret) {
		ret = bk_oob_server_setup(sock);
		close(sock);
		if (ret) {
			BK_OUT_ERROR("server setup error");
			return BK_ERR;
		}
	}
	else {
		int retry = 0;
		while (1) {
			ret = connect(sock, res->ai_addr, res->ai_addrlen);
			if (ret) {
				retry += 1;
				BK_OUT_ERROR("oob conn error (%s);retry: %d",
					strerror(errno), retry);
				if (retry > 2)
					return BK_ERR;

				sleep(1);
			}
			else {
				break;
			}

		}
		bk_bmark.srv_soc = sock;
	}

	freeaddrinfo(res);

	return BK_OK;
}

bk_status_t bk_oob_send(int socket, void* buf, size_t len) {
	BK_OUT_OOB("oob_send len: %ld", len);
	size_t msent = 0, ret;
	uint8_t* sptr = buf;
	do {
		ret = send(socket, sptr + msent, len, 0);
		if (0 > msent) {
			BK_OUT_ERROR("sock send error");
			return BK_ERR;
		}
		msent += ret;
	} while (msent < len);

	return BK_OK;
}

bk_status_t bk_oob_recv(int socket, void* buf, size_t len) {
	BK_OUT_OOB("oob_recv len: %ld", len);
	size_t mrecv = 0, ret;
	uint8_t* sptr = buf;

	do {
		ret = recv(socket, sptr + mrecv, len, 0);
		if (0 > ret) {
			BK_OUT_ERROR("sock recv error");
			return BK_ERR;
		}
		mrecv += ret;
	} while (mrecv < len);

	return BK_OK;
}

bk_status_t bk_oob_init_ranks() {
	if (bk_bmark.srv_dat) {
		bk_bmark.rank = 0;
		for (int i = 0; i < bk_bmark.bk_opts.n - 1; i++) {
			int srank = i + 1;
			if (bk_oob_send(bk_bmark.srv_dat->clients[i], &srank, sizeof(srank))) {
				BK_OUT_ERROR("rank send error");
				return BK_ERR;
			}
		}

	}
	else {
		int rrank;
		if (bk_oob_recv(bk_bmark.srv_soc, &rrank, sizeof(rrank))) {
			BK_OUT_ERROR("rank recv error");
			return BK_ERR;
		}
		bk_bmark.rank = rrank;
	}

	return BK_OK;
}


bk_status_t bk_oob_allgather(void* sendbuf, void* recvbuf, size_t slen) {
	int n = bk_bmark.bk_opts.n;
	uint8_t* rcvtmp = recvbuf;
	if (bk_bmark.srv_dat) {
		memcpy(recvbuf, sendbuf, slen);
		for (int i = 0; i < n - 1; i++) {
			if (bk_oob_recv(bk_bmark.srv_dat->clients[i], rcvtmp + (slen * (i + 1)), slen)) {
				BK_OUT_ERROR("rank %d recv error", i);
				return BK_ERR;
			}
		}
		for (int i = 0; i < n - 1; i++) {
			if (bk_oob_send(bk_bmark.srv_dat->clients[i], recvbuf, (slen * n))) {
				BK_OUT_ERROR("rank %d send error", i);
				return BK_ERR;
			}
		}
	}
	else {
		if (bk_oob_send(bk_bmark.srv_soc, sendbuf, slen)) {
			BK_OUT_ERROR("Error sending data to server");
			return BK_ERR;
		}
		if (bk_oob_recv(bk_bmark.srv_soc, recvbuf, (slen * n))) {
			BK_OUT_ERROR("Error sending data to server");
			return BK_ERR;
		}
	}
	return BK_OK;
}

bk_status_t bk_oob_barrier() {
	size_t bufsize = 1;
	uint8_t* v = alloca(sizeof(*v) * bufsize);
	uint8_t* vs = alloca(bk_bmark.bk_opts.n * bufsize);

	bk_oob_allgather(v, vs, bufsize);

	return BK_OK;
}

bk_status_t bk_init_bmark_rma() { // setup benchmark, initalize data and other requirements
	__builtin_unreachable();
}

bk_status_t bk_init_bmark_tag() { // setup benchmark, initalize data and other requirements
	size_t maxbufsize = bk_bmark.bk_opts.esize;
	bk_bmark.rbuf = calloc(maxbufsize, sizeof(uint8_t));
	bk_bmark.sbuf = calloc(maxbufsize, sizeof(uint8_t));

	return BK_OK;
}

bk_status_t bk_init_comm() { // set up the 'communicator', exchenge addresses with peers
	int ret = BK_OK;
	struct fid_ep* ep = bk_bmark.ep;
	struct fid_av* av = bk_bmark.av;
	void* peernames = NULL, * myname = NULL;
	size_t namelen = 0;

	myname = malloc(namelen);
	ret = fi_getname(&ep->fid, NULL, &namelen);
	fi_getname(&ep->fid, myname, &namelen);

	peernames = calloc(bk_bmark.bk_opts.n, namelen);
	if (BK_OK != bk_oob_allgather(myname, peernames, namelen)) {
		BK_OUT_ERROR("oob_allgather error");
		return BK_ERR;
	}

	fi_addr_t* peeraddrs = calloc(bk_bmark.bk_opts.n, sizeof(*peeraddrs));
	ret = fi_av_insert(av, peernames, bk_bmark.bk_opts.n, peeraddrs, 0, NULL);
	if (ret < 0) {
		BK_OUT_ERROR("av_insert error (%s)", fi_strerror(ret));
		return BK_ERR;
	}
	bk_bmark.peeraddrs = peeraddrs;

	if (peeraddrs)free(peeraddrs);
	if (myname)free(myname);

	return bk_init_bmark_tag();
}

bk_status_t bk_init_ep_cnnt() {
	__builtin_unreachable();
}

bk_status_t bk_init_ep_cq() {
	int ret = BK_OK;
	struct fid_cq* cq;

	struct fi_cq_attr cq_attr = { 0 };
	cq_attr.size = 128;
	cq_attr.format = FI_CQ_FORMAT_TAGGED;
	ret = fi_cq_open(bk_bmark.domain, &cq_attr, &cq, NULL);
	if (ret < 0) {
		BK_OUT_ERROR("fi_endpoint error (%s)", fi_strerror(ret));
		return BK_ERR;
	}
	bk_bmark.cq = cq;

	ret = fi_ep_bind(bk_bmark.ep, &cq->fid, FI_RECV | FI_TRANSMIT);
	if (ret < 0) {
		BK_OUT_ERROR("fi_ep_bind cq error (%s)", fi_strerror(ret));
		return BK_ERR;
	}

	return BK_OK;
}

bk_status_t bk_init_endpoint() { // initialize endpoint, attach cq and av
	int ret = BK_OK;
	struct fid_domain* domain = bk_bmark.domain;
	struct fi_info* info = bk_bmark.info;
	struct fid_ep* ep;
	struct fid_av* av;

	fi_endpoint(domain, info, &ep, NULL);
	if (ret < 0) {
		BK_OUT_ERROR("fi_endpoint error (%s)", fi_strerror(ret));
		return BK_ERR;
	}
	bk_bmark.ep = ep;

	if ((ret = bk_init_ep_cq())) { return ret; }

	struct fi_av_attr av_attr = { 0 };
	av_attr.type = FI_AV_TABLE;
	av_attr.count = bk_bmark.bk_opts.n;
	ret = fi_av_open(domain, &av_attr, &av, NULL);
	if (ret < 0) {
		BK_OUT_ERROR("fi_av_open error (%s)", fi_strerror(ret));
		return BK_ERR;
	}
	bk_bmark.av = av;

	ret = fi_ep_bind(ep, &av->fid, 0);
	if (ret < 0) {
		BK_OUT_ERROR("fi_ep_bind av error (%s)", fi_strerror(ret));
		return BK_ERR;
	}

	ret = fi_enable(ep);
	if (ret < 0) {
		BK_OUT_ERROR("fi_enable error (%s)", fi_strerror(ret));
		return BK_ERR;
	}
	return bk_init_comm();
}


bk_status_t bk_init_fabric() { // initialize fabric, call getinfo and establish fabric & domain
	int ret = 0;
	struct fi_info* info, * hints = bk_bmark.hints;
	struct fid_fabric* fabric;
	struct fid_domain* domain;

	// fi_getinfo(FI_VERSION(1, 17), bk_opt->srv_addr, bk_opt->port, 0, hints, &info);
	ret = fi_getinfo(FI_VERSION(1, 15), NULL, NULL, 0, hints, &info);
	if (ret < 0) {
		BK_OUT_ERROR("fi_getinfo error (%s)", fi_strerror(ret));
		return BK_ERR;
	}
	bk_bmark.info = info;

	BK_OUT_INFO("Selected provider: %s (%s)", info->fabric_attr->prov_name, info->domain_attr->name);
	BK_OUT_INFO("max_tx: %ld, max_rx: %ld", info->domain_attr->max_ep_tx_ctx, info->domain_attr->max_ep_rx_ctx);
	BK_OUT_INFO("max_msg_size: %ld", info->ep_attr->max_msg_size);

	ret = fi_fabric(info->fabric_attr, &fabric, NULL);
	if (ret < 0) {
		BK_OUT_ERROR("fi_fabric error (%s)", fi_strerror(ret));
		return BK_ERR;
	}
	bk_bmark.fabric = fabric;
	fflush(stdout);

	ret = fi_domain(fabric, info, &domain, NULL);
	if (ret < 0) {
		BK_OUT_ERROR("fi_domain error (%s)", fi_strerror(ret));
		return BK_ERR;
	}
	bk_bmark.domain = domain;
	fflush(stdout);

	return bk_init_endpoint();
}