#include <rdma/fabric.h>
#include "bk_libfab.h"

int main(int argc, char* argv[]) {
	int res;
	bk_bmark_t bk_bmark_data = { 0 };
	int ret;

	bk_init_opts(&bk_bmark_data.bk_opts);
	ret = bk_parse_args(argc, argv, &bk_bmark_data.bk_opts);
	if (BK_OK != ret) {
		return ret;
	}

	bk_print_info(&bk_bmark_data);

	res = bk_oob_init(&bk_bmark_data);
	if (res) {
		return BK_ERR;
	}
	bk_oob_init_ranks(&bk_bmark_data);
	BK_OUT("OOB init, pid: %d got rank %d", getpid(), bk_bmark_data.rank);


	bk_bmark_data.hints = fi_allocinfo();
	if (NULL == bk_bmark_data.hints) {
		BK_OUT_ERR("Out of resources");
		return BK_ERR;
	}

	bk_bmark_data.hints->caps = FI_TAGGED | FI_RMA;

	bk_init_fabric(&bk_bmark_data);

	fflush(stdout);
	fflush(stderr);

	bk_cleanup(&bk_bmark_data);

	return 0;
}