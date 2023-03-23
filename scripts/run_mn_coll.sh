#!/cvmfs/soft.computecanada.ca/gentoo/2020/bin/bash

set -e

WDIR="$HOME/bk_libfab"
bin="$WDIR/src/bin/bk_libfab_coll"

init_wdir="pushd $WDIR && source initenv.bash"

# BK_HOSTNAMES=("cedar1" "cedar5")
BK_HOSTNAMES=("gra-login1" "gra-login2")
BK_SERVER="${BK_HOSTNAMES[0]}"
BK_N="${#BK_HOSTNAMES[@]}"

echo "server: $BK_SERVER"

BK_FLAGS="-s $BK_SERVER -n $BK_N -v 6 -m $((1<<10)):$((1<<20)) -i 10 -w 1"

for host in "${BK_HOSTNAMES[@]}"; do 
	# cmd="$init_wdir && $bin -s $BK_SERVER -n $BK_N -v 6 -m $((1<<7)):$((1<<14))"
	# cmd="$init_wdir && $bin -s $BK_SERVER -n $BK_N -v 4 -m $((1<<10)):$((1<<20))"
	cmd="$init_wdir && $bin $BK_FLAGS"
	ssh $host $cmd &
done

jobs
wait

