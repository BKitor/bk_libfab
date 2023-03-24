#!/cvmfs/soft.computecanada.ca/gentoo/2020/bin/bash

source initenv.bash

set -e

pushd third_party/libfabric
./autogen.sh
./configure --prefix="$BK_LIBFAB/build" \
	--enable-efa=no \
	--enable-psm=no \
	--enable-psm2=no \
	--enable-psm3=no \
	--enable-opx=no \
	--enable-verbs=yes \
	--enable-debug

make clean
make -j 8 
make install

popd
