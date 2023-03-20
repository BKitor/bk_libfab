#!/cvmfs/soft.computecanada.ca/gentoo/2020/bin/bash

source initenv.bash

set -e

pushd libfabric
./configure --prefix="$BK_LIBFAB/build" \
	--with-cuda=$EBROOTCUDA \
	--enable-efa=no \
	--enable-psm2=yes \
	--enable-opx=no \
	--enable-verbs=yes \
	--enable-debug

make clean
make -j 8 
make install

popd
