#
# -*- coding: utf-8 -*-
#
# cdm-flowparser - python control flow parser used in Codimension to
# automatically generate a flowchart like diagram
# Copyright (C) 2010-2015  Sergey Satskiy <sergey.satskiy@gmail.com>
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


from distutils.core import setup, Extension
import os

long_description = """Python language control flow parser.
Written as a part of the Codimension project, this parser
aims at pulling all the necessery data to build a control
flow diagram."""

try:
	version = os.environ['CDM_PROJECT_BUILD_VERSION']
except KeyError:
	version = 'trunk'

setup( name = 'cdmcfparser',
       description = 'Codimension Python Control Flow Parser',
       long_description = long_description,
       version = version,
       author = 'Sergey Satskiy',
       author_email = 'sergey.satskiy@gmail.com',
       url = 'http://satsky.spb.ru/codimension/',
       license = 'GPLv3',
       classifiers = [
           'Development Status :: 5 - Production/Stable',
           'Intended Audience :: Developers',
           'License :: OSI Approved :: GNU General Public License (GPL)',
           'Operating System :: POSIX :: Linux',
           'Programming Language :: C',
           'Programming Language :: Python',
           'Topic :: Software Development :: Libraries :: Python Modules'],
       platforms = [ 'any' ],
       py_modules  = [ 'cdmcf' ],
       ext_modules = [ Extension( 'cdmcf',
                                  sources = [ 'src/cflowmodule.cpp',
                                              'src/cflowfragments.cpp',
                                              'src/cflowutils.cpp',
                                              'src/cflowparser.cpp',
                                              'src/cflowcomments.cpp',
                                              'thirdparty/pycxx/Src/cxxsupport.cxx',
                                              'thirdparty/pycxx/Src/cxx_extensions.cxx',
                                              'thirdparty/pycxx/Src/IndirectPythonInterface.cxx',
                                              'thirdparty/pycxx/Src/cxxextensions.c',
                                              'thirdparty/pycxx/Src/cxx_exceptions.cxx' ],
                                  include_dirs = [ 'thirdparty/pycxx', 'thirdparty/pycxx/Src', 'src' ],
                                  extra_compile_args = [ '-Wno-unused', '-fomit-frame-pointer',
                                                         '-DCDM_CF_PARSER_VERSION="' + version + '"',
                                                         '-ffast-math',
                                                         '-O2',
                                                         '-DPYCXX_PYTHON_2TO3' ],
                                ) ] )

