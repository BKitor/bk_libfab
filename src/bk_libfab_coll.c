#include <rdma/fi_tagged.h>
#include "bk_libfab.h"

struct fi_context sctx = { 0 }, rctx = { 0 };

bk_bm_def_t bk_iter_def_arr[] = {
	{"barrier", 	bk_barrier_iter_fn,		NULL},
	{"oob_barrier", bk_oob_barrier_iter_fn,	NULL},
	{"tag_ring", 	bk_tag_ring_iter_fn, 	NULL},
	{"rma_ring", 	bk_rma_ring_iter_fn,	NULL},
};
size_t bk_iter_def_arr_len = 4;

static inline bk_status_t bk_barrier() {
	int ret = BK_OK;
	struct fid_ep* ep = bk_bmark.ep;
	int p = bk_bmark.rank, n = bk_bmark.bk_opts.n;
	uint64_t rbuf, sbuf;
	for (int m = 1; m < n; m <<= 1) {
		int speer = (p + m) % n;
		int rpeer = (p + n + m) % n;
		ret = fi_trecv(ep, &rbuf, sizeof(rbuf), NULL, rpeer, 0, 0, &rctx);
		if (ret) { BK_OUT_ERROR("error posting fi_trecv in bk_barrier (%s)", fi_strerror(ret)); return ret; }
		ret = fi_tsend(ep, &sbuf, sizeof(rbuf), NULL, speer, 0, &sctx);
		if (ret) { BK_OUT_ERROR("error posting fi_tsend in bk_barrier (%s)", fi_strerror(ret)); return ret; }
		bk_wait_cq();
		bk_wait_cq();
	}
	return BK_OK;
}

bk_status_t bk_barrier_iter_fn(bk_mb_iter_t* i) {
	bk_barrier();
	clock_gettime(CLOCK_MONOTONIC, &(i->cstart));
	bk_barrier();
	clock_gettime(CLOCK_MONOTONIC, &(i->cend));
	return BK_OK;
}

bk_status_t bk_oob_barrier_iter_fn(bk_mb_iter_t* i) {
	bk_barrier();
	clock_gettime(CLOCK_MONOTONIC, &(i->cstart));
	bk_oob_barrier();
	clock_gettime(CLOCK_MONOTONIC, &(i->cend));
	return BK_OK;

}

bk_status_t bk_tag_ring_iter_fn(bk_mb_iter_t* i) {
	int r = bk_bmark.rank, n = bk_bmark.bk_opts.n, ret = BK_OK;
	struct fid_ep* ep = bk_bmark.ep;
	size_t msize = i->msize;
	void* rbuf = bk_bmark.rbuf;
	void* sbuf = bk_bmark.sbuf;

	fi_addr_t speer = (r + n + 1) % n;
	fi_addr_t rpeer = (r + n - 1) % n;

	bk_barrier();
	clock_gettime(CLOCK_MONOTONIC, &(i->cstart));

	ret = fi_trecv(ep, rbuf, msize, NULL, rpeer, 0, 0, &rctx);
	BK_OUT_DEBUG("post recv s: %ld, p: %ld, ret: %d", msize, rpeer, ret);
	if (__builtin_expect(ret, 0)) {
		BK_OUT_ERROR("Posting trecv failed %ld (%s)", msize, fi_strerror(ret));
		return BK_ERR;
	}
	ret = fi_tsend(ep, sbuf, msize, NULL, speer, 0, &sctx);
	BK_OUT_DEBUG("post send s: %ld, p: %ld, ret: %d", msize, speer, ret);
	if (__builtin_expect(ret, 0)) {
		BK_OUT_ERROR("Posting tsend failed %ld (%s)", msize, fi_strerror(ret));
		return BK_ERR;
	}
	bk_wait_cq();
	bk_wait_cq();

	clock_gettime(CLOCK_MONOTONIC, &(i->cend));

	return BK_OK;
}

bk_status_t bk_rma_ring_iter_fn(bk_mb_iter_t* i) {
	__builtin_unreachable();
	// 	fid_ep = bk_bmark.ep;

	// 	fi_addr_t speer = (r + n + 1) % n;
	// 	fi_addr_t rpeer = (r + n - 1) % n;

	// 	fi_read(ep, );
	// 	fi_write(ep, );

	// 	return BK_OK;
}


bk_status_t bk_bmark_harness(bk_iter_fn_t iter_fn) {
	int ret = BK_OK;
	bk_mb_iter_t i;
	for (size_t msize = bk_bmark.bk_opts.ssize; msize < bk_bmark.bk_opts.esize; msize *= 2) {
		double ttotal = 0.0, tmin = 0.0, tmax = 0.0, itime, tavg;
		for (int32_t iter = 0; iter < bk_bmark.bk_opts.iters + bk_bmark.bk_opts.warmups;iter++) {

			i.msize = msize;

			if (BK_OK != (ret = iter_fn(&i))) {
				BK_OUT_ERROR("iter_fn return BK_ERR");
				return ret;
			}

			if (iter > bk_bmark.bk_opts.warmups) {
				itime = ((double)(i.cend.tv_nsec - i.cstart.tv_nsec)) * 1e-3;
				ttotal += itime;
				tmin = (tmin == 0 || itime < tmin) ? itime : tmin;
				tmax = (itime > tmax) ? itime : tmax;
			}
		}
		tavg = ttotal / (double)bk_bmark.bk_opts.iters;
		if (0 == bk_bmark.rank)BK_OUT_INFO("size:%ld avg_time:%.3f min_time:%.3f max_time:%.3f total_time:%.3f", msize, tavg, tmin, tmax, ttotal);
	}
	return BK_OK;
}

int main(int argc, char* argv[]) {
	bk_status_t ret = BK_OK;
	bk_bm_def_t bm_lst_head = { 0 };

	bk_bmark_construct(&bk_bmark);

	ret = bk_parse_args(argc, argv, &bk_bmark.bk_opts);
	if (BK_OK != ret) {
		return ret;
	}

	if ((ret = bk_parse_def_csl(&bm_lst_head))) { return ret; }

	if (!bm_lst_head.next) {
		BK_OUT_ERROR("No benchmarks specified");
		return BK_ERR;
	}

	bk_print_info(&bm_lst_head);

	ret = bk_oob_init();
	if (BK_OK != ret) {
		return BK_ERR;
	}
	bk_oob_init_ranks();
	BK_OUT_INFO("OOB init, pid: %d got rank %d", getpid(), bk_bmark.rank);

	bk_bmark.hints = fi_allocinfo();
	if (NULL == bk_bmark.hints) {
		BK_OUT_ERROR("Out of resources");
		return BK_ERR;
	}

	bk_bmark.hints->caps = FI_TAGGED | FI_ATOMIC | FI_RMA;
	bk_bmark.hints->mode = FI_CONTEXT;
	bk_bmark.hints->ep_attr->type = FI_EP_RDM;
	bk_bmark.hints->fabric_attr->prov_name = "psm2";

	ret = bk_init_fabric();
	if (BK_OK != ret) { return BK_ERR; };

	bk_bm_def_t* bm_lst = &bm_lst_head;
	while (bm_lst->next) {
		bm_lst = bm_lst->next;
		if (0 == bk_bmark.rank)BK_OUT_INFO("BMARK: %s", bm_lst->name);
		ret = bk_bmark_harness(bm_lst->fn);
		if (BK_OK != ret) { return BK_ERR; };
	}

	bk_oob_barrier();
	bk_cleanup();

	return 0;
}
