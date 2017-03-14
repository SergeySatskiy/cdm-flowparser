#!/bin/bash
set -e
set -o pipefail

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

for PYTHON in \
    python2.6 \
    python2.7 \
    python3.3 \
    python3.4 \
    python3.5 \
    python3.6 \
    ;
do
    if which $PYTHON >/dev/null
    then
        echo "Info: Found ${PYTHON}"
        ${PYTHON} setup_makefile.py ${OS} tmp-$PYTHON.mak
        make -f tmp-$PYTHON.mak clean 2>&1 | tee tmp-$PYTHON.log
        make -f tmp-$PYTHON.mak test 2>&1 | tee -a tmp-$PYTHON.log
    fi
done
