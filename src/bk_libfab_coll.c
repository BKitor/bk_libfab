#include <rdma/fi_tagged.h>
#include "bk_libfab.h"

struct fi_context sctx = { 0 }, rctx = { 0 };

bk_status_t bk_ring_exch() {
	int r = bk_bmark.rank, n = bk_bmark.bk_opts.n, ret = BK_OK;
	size_t maxbufsize = bk_bmark.bk_opts.esize;
	struct fid_ep* ep = bk_bmark.ep;

	uint64_t* sbuf = malloc(maxbufsize);
	uint64_t* rbuf = malloc(maxbufsize);

	fi_addr_t speer = (r + n + 1) % n;
	fi_addr_t rpeer = (r + n - 1) % n;

	for (size_t msize = bk_bmark.bk_opts.ssize; msize < bk_bmark.bk_opts.esize; msize *= 2) {
		struct timespec cstart, cend;
		double ttotal = 0.0, tmin = 0.0, tmax = 0.0, itime, tavg;
		for (int32_t iter = 0; iter < bk_bmark.bk_opts.iters + bk_bmark.bk_opts.warmups;iter++) {

			clock_gettime(CLOCK_MONOTONIC, &cstart);

			ret = fi_trecv(ep, rbuf, msize, NULL, rpeer, 0, 0, &rctx);
			BK_OUT_DEBUG("post recv iter: %d, s: %ld, p: %ld, ret: %d", iter, msize, rpeer, ret);
			if (__builtin_expect(ret, 0)) {
				BK_OUT_ERROR("Posting trecv failed %ld (%s)", msize, fi_strerror(ret));
				return BK_ERR;
			}
			ret = fi_tsend(ep, sbuf, msize, NULL, speer, 0, &sctx);
			BK_OUT_DEBUG("post send iter: %d, s: %ld, p: %ld, ret: %d", iter, msize, speer, ret);
			if (__builtin_expect(ret, 0)) {
				BK_OUT_ERROR("Posting tsend failed %ld (%s)", msize, fi_strerror(ret));
				return BK_ERR;
			}
			bk_wait_cq();
			bk_wait_cq();

			clock_gettime(CLOCK_MONOTONIC, &cend);

			if (iter > bk_bmark.bk_opts.warmups) {
				itime = ((double)(cend.tv_nsec - cstart.tv_nsec)) * 1e-3;
				ttotal += itime;
				tmin = (tmin == 0 || itime < tmin) ? itime : tmin;
				tmax = (itime > tmax) ? itime : tmax;
			}
		}
		tavg = ttotal / (double)bk_bmark.bk_opts.iters;
		if (0 == r)BK_OUT_INFO("size:%ld total_time:%.3f avg_time:%.3f min_time:%.3f max_time:%.3f", msize, ttotal, tavg, tmin, tmax);
	}

	if (sbuf)free(sbuf);
	if (rbuf)free(rbuf);

	return BK_OK;
}

int main(int argc, char* argv[]) {
	bk_status_t ret = BK_OK;

	bk_init_bmark(&bk_bmark);

	ret = bk_parse_args(argc, argv, &bk_bmark.bk_opts);
	if (BK_OK != ret) {
		return ret;
	}

	bk_print_info();

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
	// bk_bmark.hints->mode = FI_CONTEXT;
	bk_bmark.hints->mode = FI_CONTEXT;
	bk_bmark.hints->ep_attr->type = FI_EP_RDM;
	// bk_bmark.hints->domain_attr->av_type = FI_AV_MAP;
	bk_bmark.hints->fabric_attr->prov_name = "verbs";

	ret = bk_init_fabric();
	if (BK_OK != ret) { return BK_ERR; };

	// ret = bk_ring_exch();
	// if (BK_OK != ret) { return BK_ERR; };

	bk_oob_barrier();
	bk_cleanup();

	return 0;
}