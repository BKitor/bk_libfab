ml --force purge 
if [[ "$HOSTNAME" = "nia-login"* ]];then # on a niagara login node
	ml load CCEnv
fi

ml load StdEnv/2020
ml unload ucx libfabric openmpi imkl
# ml load libfabric/1.15.1 gcc/9.3.0 # cuda/11.4 opa-psm2/11.2.206
ml load gcc/9.3.0 # cuda/11.4 opa-psm2/11.2.206

export BK_LIBFAB="$PWD"
export PATH="$BK_LIBFAB/build/bin:$PATH"
export MANPATH="$BK_LIBFAB/build/share/man:$MANPATH"
export CPATH="$BK_LIBFAB/build/include:$CPATH"
export LD_LIBRARY_PATH="$BK_LIBFAB/build/lib:$LD_LIBRARY_PATH"

# export BK_VALGRIND_DIR="/home/bkitor/valgrind-3.20.0"
# export PATH="$BK_VALGRIND_DIR/build/bin:$PATH"
# export MANPATH="$BK_VALGRIND_DIR/build/share/man:$MANPATH"
# export CPATH="$BK_VALGRIND_DIR/build/include:$CPATH"
# export LD_LIBRARY_PATH="$BK_VALGRIND_DIR/build/lib:$LD_LIBRARY_PATH"

