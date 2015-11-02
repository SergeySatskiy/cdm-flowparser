#
# cdm-flowparser - python control flow parser used in Codimension to
# automatically generate a flowchart like diagram
# Copyright (C) 2010  Sergey Satskiy <sergey.satskiy@gmail.com>
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

VERSION=trunk

PYCXX_DIR=../thirdparty/pycxx
PYCXX_INCLUDE=-I${PYCXX_DIR} -I${PYCXX_DIR}/Src
PYTHON_INCLUDE=$(shell python -c 'import distutils.sysconfig; print distutils.sysconfig.get_python_inc()')
INCLUDE=${PYCXX_INCLUDE} -I${PYTHON_INCLUDE}

VERSION_DEFINES=-DCDM_CF_PARSER_VERION=\"${VERSION}\"
FLAGS=-Wall -O2 -fno-gcse -ffast-math -fomit-frame-pointer -fPIC -fexceptions -frtti -DNDEBUG -D_GNU_SOURCE ${VERSION_DEFINES}

PYCXX_OBJ_FILES=${PYCXX_DIR}/Src/cxxsupport.o ${PYCXX_DIR}/Src/cxx_extensions.o \
                ${PYCXX_DIR}/Src/IndirectPythonInterface.o ${PYCXX_DIR}/Src/cxxextensions.o
PYCXX_SRC_FILES=${PYCXX_DIR}/Src/cxxsupport.cxx ${PYCXX_DIR}/Src/cxx_extensions.cxx \
                ${PYCXX_DIR}/Src/IndirectPythonInterface.cxx ${PYCXX_DIR}/Src/cxxextensions.c
CDM_SRC_FILES=cflowmodule.cpp cflowfragments.cpp cflowutils.cpp cflowparser.cpp cflowcomments.cpp
CDM_INC_FILES=cflowmodule.hpp cflowfragments.hpp cflowutils.hpp cflowparser.hpp cflowcomments.hpp


all: $(CDM_SRC_FILES) $(CDM_INC_FILES) $(PYCXX_SRC_FILES)
	cd .. && python setup.py build_ext --inplace
	g++ ${FLAGS} -o tree  tree.cpp -I ${PYTHON_INCLUDE} -L/opt/python-2.7/lib/ -lpython2.7
	g++ -o comments comments.cpp cflowcomments.cpp

clean:
	rm -rf *.o core.* *.pyc tree comments ../build ../cdmcf.so

check:
	PYTHONPATH=../:${PYTHONPATH} ../tests/ut.py

