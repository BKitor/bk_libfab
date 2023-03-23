#!/cvmfs/soft.computecanada.ca/gentoo/2020/bin/bash

source initenv.bash

set -e

if [ ! -d ./libfabric ]; then
	git clone --branch v1.17.1 https://github.com/ofiwg/libfabric
	pushd libfabric
	./autogen.sh
	popd

fi

pushd libfabric
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
