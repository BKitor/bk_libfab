#!/cvmfs/soft.computecanada.ca/gentoo/2020/bin/bash

source initenv.bash

set -e

pushd libfabric
./configure --prefix="$BK_LIBFAB/build" \
	--enable-efa=no \
	--with-cuda=$EBROOTCUDA \
	--enable-psm2=yes \
	--enable-verbs=yes

make -j 8 
make install

popd
