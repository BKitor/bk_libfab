#!/cvmfs/soft.computecanada.ca/gentoo/2020/bin/bash

set -e

WDIR="$HOME/bk_libfab"
bin="$WDIR/src/bin/bk_libfab_coll"
valgrind_bin="$WDIR/build/valgrind"

init_wdir="pushd $WDIR && source initenv.bash"

function print_help {
	echo "run_mn_coll.sh Options: "
	echo "	-b		Benchmarks ()"
	echo "	-i		Itterations (10)"
	echo "	-m		Message range ($(1<<10):$(1<<15))"
	echo "	-v		Verbosity (4)"
	echo "	-w 		Warmups (1)"
}

BK_EN_VALGRIND=false
BK_BMARKS="barrier,tag_ring"
BK_VERBOSITY="4"
BK_MSIZES="$((1<<10)):$((1<<15))"
BK_ITERS="10"
BK_WARMUPS="1"

while getopts "b:i:m:Vv:w:" opt; do
	case $opt in 
		b)
			BK_BMARKS=$OPTARG
			;;
		i)
			BK_ITERS=$OPTARG
			;;
		m)
			BK_MSIZES=$OPTARG
			;;
		V)
			BK_EN_VALGRIND=true
			;;
		v)
			BK_VERBOSITY=$OPTARG
			;;
		w)
			BK_WARMUPS=$OPTARG
			;;
	esac
done

if $BK_EN_VALGRIND; then
	bin="valgrind $bin"
fi

# BK_HOSTNAMES=("cedar1" "cedar5")
BK_HOSTNAMES=("cedar5" "cedar1")
# BK_HOSTNAMES=("gra-login1" "gra-login2")
# BK_HOSTNAMES=("cdr2408" "cdr2430")
BK_SERVER="${BK_HOSTNAMES[0]}"
BK_N="${#BK_HOSTNAMES[@]}"

echo "server: $BK_SERVER"

BK_FLAGS="-s $BK_SERVER -n $BK_N -v $BK_VERBOSITY -m $BK_MSIZES -i $BK_ITERS -w $BK_WARMUPS -b $BK_BMARKS"

for host in "${BK_HOSTNAMES[@]}"; do 
	# cmd="$init_wdir && $bin -s $BK_SERVER -n $BK_N -v 6 -m $((1<<7)):$((1<<14))"
	# cmd="$init_wdir && $bin -s $BK_SERVER -n $BK_N -v 4 -m $((1<<10)):$((1<<20))"
	cmd="$init_wdir && $bin $BK_FLAGS"
	ssh $host $cmd &
done

jobs
wait

