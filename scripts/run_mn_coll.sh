#!/cvmfs/soft.computecanada.ca/gentoo/2020/bin/bash

set -e

WDIR="$HOME/libfab_proj"
bin="$WDIR/src/bin/bk_libfab_coll"

init_wdir="pushd $WDIR && source initenv.bash"

# BK_HOSTNAMES=("cedar1" "cedar5")
BK_HOSTNAMES=("cedar5" "cedar1")
BK_SERVER="${BK_HOSTNAMES[0]}"
BK_N="${#BK_HOSTNAMES[@]}"

echo "server: $BK_SERVER"

for host in "${BK_HOSTNAMES[@]}"; do 
	cmd="$init_wdir && $bin -s $BK_SERVER -n $BK_N"
	ssh $host $cmd &
done

jobs
wait

