#!/bin/bash
set -x
set -e
set -o pipefail

PYTHON=${1? python exe}

case "$( uname )" in
Darwin)
    OS=macosx
    ;;
Linux):
    OS=linux
    ;;
*)
    echo Unknown OS assuming Linux
    OS=linux
    ;;
esac

PYTHON_BASE=$(basename ${PYTHON})

${PYTHON} setup_makefile.py ${OS} tmp-${PYTHON_BASE}-unlimited-api.mak
make -f tmp-${PYTHON_BASE}-unlimited-api.mak clean 2>&1 | tee tmp-${PYTHON_BASE}-unlimited-api.log
make -f tmp-${PYTHON_BASE}-unlimited-api.mak test 2>&1 | tee -a tmp-${PYTHON_BASE}-unlimited-api.log
