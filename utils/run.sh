#!/bin/bash

set -e

cd $(dirname "${0}")/..

# ---------------------------------------------------------------------------------------------------------------------
# check target

target="${1}"

if [ -z "${target}" ]; then
    echo "usage: ${0} <target>"
    exit 1
fi

# ---------------------------------------------------------------------------------------------------------------------
# read next CLI arg

shift

# ---------------------------------------------------------------------------------------------------------------------
# import env

export PAWPAW_SKIP_LTO=1
source PawPaw/local.env ${target}

# ---------------------------------------------------------------------------------------------------------------------
# expand alias if needed

PRE=""

if [ "_${1}" = "_cmake" ] && [ "_${2}" = "_-S" ]; then
    PRE="${CMAKE} -S"
    shift
    shift
elif [ "_${1}" = "_python3" ]; then
    PRE="${EXE_WRAPPER} ${PAWPAW_PREFIX}/bin/python3${APP_EXT}"
    shift
fi

if [ "${CROSS_COMPILING}" -eq 0 ] && [ "${LINUX}" -eq 1 ]; then
    export LD_LIBRARY_PATH="${PAWPAW_PREFIX}/lib"
fi

# export PYTHON_PATH=""

# ---------------------------------------------------------------------------------------------------------------------
# run command

${PRE} $@

# ---------------------------------------------------------------------------------------------------------------------