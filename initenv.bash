ml --force purge 
ml load StdEnv/2020
ml unload ucx libfabric openmpi imkl
ml load gcc/9.3.0 # cuda/11.4 opa-psm2/11.2.206

export BK_LIBFAB="$HOME/libfab_proj"
export PATH="$BK_LIBFAB/build/bin:$PATH"
export MANPATH="$BK_LIBFAB/build/share/man:$MANPATH"
export CPATH="$BK_LIBFAB/build/include:$CPATH"
export LD_LIBRARY_PATH="$BK_LIBFAB/build/lib:$LD_LIBRARY_PATH"
