#
# cdm-flowparser - python control flow parser used in Codimension to
# automatically generate a flowchart like diagram
# Copyright (C) 2010 - 2017  Sergey Satskiy <sergey.satskiy@gmail.com>
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.
#

# TODO:
# - dependencies between cpp and hpp files are not here
# - C flags should not have -frtti
# - move object files to a build dir


.PHONY: all clean check


# The python-config is not a very reliable choice to get the compiler
# flags information. The name may depend from an interpreter version, e.g. it
# could be python3-config or it could be picked system wide but not from a
# virtual environment. So instead a python interpreter itself is used.

VERSION=${CDM_PROJECT_BUILD_VERSION:-trunk}

PYTHON_INCLUDE=$(shell python -c 'import distutils.sysconfig; print(distutils.sysconfig.get_python_inc())')
PYTHON_LIBS_PATH=$(shell python -c 'import distutils.sysconfig; print(distutils.sysconfig.get_config_var("LIBDIR"))')
PYCXX_DIR=../thirdparty/pycxx
PYCXX_INCLUDE=-I${PYCXX_DIR} -I${PYCXX_DIR}/Src
INCLUDE=${PYCXX_INCLUDE} ${PYTHON_INCLUDE}

# Typical python-config --ldflags output:
# -lpython3.5m -lpthread -ldl -lutil -lm -Xlinker -export-dynamic

# This will produce: -Xlinker -export-dynamic
LINK_FOR_SHARED=$(shell python -c 'import distutils.sysconfig; print(distutils.sysconfig.get_config_var("LINKFORSHARED"))')

# This will produce: -lpthread -ldl  -lutil
LIBS=$(shell python -c 'import distutils.sysconfig; print(distutils.sysconfig.get_config_var("LIBS"))')

# This will produce: -L. -lpython3.5m
BLD_LIBRARY=$(shell python -c 'import distutils.sysconfig; print(distutils.sysconfig.get_config_var("BLDLIBRARY"))')

VERSION_DEFINES=-DCDM_CF_PARSER_VERSION=\"${VERSION}\"
FLAGS=-Wall -O2 -fno-gcse -ffast-math -fomit-frame-pointer -fPIC -fexceptions -frtti -DNDEBUG -D_GNU_SOURCE ${VERSION_DEFINES}

# MacOS g++ does not like -rpath ...
OS_NAME=$(shell uname -s)
ifeq ($(OS_NAME),Linux)
FLAGS += -Wl,-rpath=${PYTHON_LIBS_PATH}
endif

PYCXX_OBJ_FILES=${PYCXX_DIR}/Src/cxxsupport.o ${PYCXX_DIR}/Src/cxx_extensions.o \
                ${PYCXX_DIR}/Src/IndirectPythonInterface.o ${PYCXX_DIR}/Src/cxxextensions.o \
                ${PYCXX_DIR}/Src/cxx_exceptions.o
PYCXX_SRC_FILES=${PYCXX_DIR}/Src/cxxsupport.cxx ${PYCXX_DIR}/Src/cxx_extensions.cxx \
                ${PYCXX_DIR}/Src/IndirectPythonInterface.cxx ${PYCXX_DIR}/Src/cxxextensions.c \
                ${PYCXX_DIR}/Src/cxx_exceptions.cxx
CDM_SRC_FILES=cflowmodule.cpp cflowfragments.cpp cflowutils.cpp cflowparser.cpp cflowcomments.cpp
CDM_INC_FILES=cflowmodule.hpp cflowfragments.hpp cflowutils.hpp cflowparser.hpp cflowcomments.hpp


all: $(CDM_SRC_FILES) $(CDM_INC_FILES) $(PYCXX_SRC_FILES)
	cd .. && CDM_PROJECT_BUILD_VERSION=$(CDM_PROJECT_BUILD_VERSION) python setup.py build_ext --inplace
	g++ ${FLAGS} -o tree  tree.cpp -I${PYTHON_INCLUDE} -L${PYTHON_LIBS_PATH} ${BLD_LIBRARY} ${LIBS} ${LINK_FOR_SHARED}
	g++ -o comments comments.cpp cflowcomments.cpp

clean:
	rm -rf *.o core.* *.pyc tree comments ../build ../cdmcfparser.so

check:
	PYTHONPATH=../:${PYTHONPATH} ../tests/ut.py

localinstall:
	cd .. && CDM_PROJECT_BUILD_VERSION=$(CDM_PROJECT_BUILD_VERSION) python setup.py install --user

