#!/bin/bash
set -x
set -e
set -o pipefail

for PYTHON in \
    python2.6 \
    python2.7 \
    python3.3 \
    python3.4 \
    python3.5 \
    python3.6 \
    python3.7 \
    python3.8 \
    python3.9 \
    ;
do
    if which $PYTHON >/dev/null
    then
        echo "Info: Found ${PYTHON}"
        ./build-unlimited-api.sh ${PYTHON}

        case "${PYTHON}" in
        python3.3)
            ;;
        python3.*)
            ./build-limited-api.sh ${PYTHON} ${PYTHON#python}
            ;;
        esac
    fi
done
