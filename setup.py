# -*- coding: utf-8 -*-
#
# cdm-flowparser - python control flow parser used in Codimension to
# automatically generate a flowchart like diagram
# Copyright (C) 2010-2017  Sergey Satskiy <sergey.satskiy@gmail.com>
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


import sys
import os.path
import platform
from setuptools import setup, Extension

description = 'Python language control flow parser. ' \
    'Written as a part of the Codimension project, this parser ' \
    'aims at pulling all the necessery data to build a control ' \
    'flow diagram.'

try:
    version = os.environ['CDM_PROJECT_BUILD_VERSION']
except KeyError:
    version = 'trunk'
    if os.path.exists('cdmcfparserversion.py'):
        try:
            import cdmcfparserversion
            version = cdmcfparserversion.version
        except:
            pass

try:
    import pypandoc
    converted = pypandoc.convert_file('README.md', 'rst').splitlines()
    no_travis = [line for line in converted if 'travis-ci.org' not in line]
    long_description = '\n'.join(no_travis)

    # Pypi index does not like this link
    long_description = long_description.replace('|Build Status|', '')
except Exception as exc:
    print('pypandoc package is not installed: the markdown '
          'README.md convertion to rst failed: ' + str(exc), file=sys.stderr)
    import io
    # pandoc is not installed, fallback to using raw contents
    with io.open('README.md', encoding='utf-8') as f:
        long_description = f.read()


extraLinkArgs = None
if platform.system().lower() == 'linux':
    # On some systems there are many compilers installed and a wrong version of
    # the libstdc++ may be picked up at run-time. So it is safer to link it
    # statically. The overall size is obviously increased but not dramatically.
    extraLinkArgs = ['-static-libstdc++']


# install_requires=['pypandoc'] could be added but really it needs to only
# at the time of submitting a package to Pypi so it is excluded from the
# dependencies
setup(name='cdmcfparser',
      description=description,
      long_description=long_description,
      version=version,
      author='Sergey Satskiy',
      author_email='sergey.satskiy@gmail.com',
      url='https://github.com/SergeySatskiy/cdm-flowparser',
      license='GPLv3',
      classifiers=[
            'Development Status :: 5 - Production/Stable',
            'Intended Audience :: Developers',
            'License :: OSI Approved :: GNU General Public License (GPL)',
            'Operating System :: POSIX :: Linux',
            'Programming Language :: C++',
            'Programming Language :: Python :: 3',
            'Topic :: Software Development :: Libraries :: Python Modules'],
       platforms=['any'],
       py_modules=['cdmcfparser'],
       ext_modules=[Extension('cdmcfparser',
                              sources=['src/cflowmodule.cpp',
                                       'src/cflowfragments.cpp',
                                       'src/cflowutils.cpp',
                                       'src/cflowparser.cpp',
                                       'src/cflowcomments.cpp',
                                       'thirdparty/pycxx/Src/cxxsupport.cxx',
                                       'thirdparty/pycxx/Src/cxx_extensions.cxx',
                                       'thirdparty/pycxx/Src/IndirectPythonInterface.cxx',
                                       'thirdparty/pycxx/Src/cxxextensions.c',
                                       'thirdparty/pycxx/Src/cxx_exceptions.cxx'],
                              depends=['src/cflowcomments.hpp',
                                       'src/cflowdocs.hpp',
                                       'src/cflowfragments.hpp',
                                       'src/cflowfragmenttypes.hpp',
                                       'src/cflowmodule.hpp',
                                       'src/cflowparser.hpp',
                                       'src/cflowutils.hpp',
                                       'src/cflowversion.hpp',
                                       'thirdparty/pycxx/Src/Python3/cxx_exceptions.cxx',
                                       'thirdparty/pycxx/Src/Python3/cxx_extensions.cxx',
                                       'thirdparty/pycxx/Src/Python3/cxxextensions.c',
                                       'thirdparty/pycxx/Src/Python3/cxxsupport.cxx',
                                       'thirdparty/pycxx/CXX/Config.hxx',
                                       'thirdparty/pycxx/CXX/CxxDebug.hxx',
                                       'thirdparty/pycxx/CXX/Exception.hxx',
                                       'thirdparty/pycxx/CXX/Extensions.hxx',
                                       'thirdparty/pycxx/CXX/IndirectPythonInterface.hxx',
                                       'thirdparty/pycxx/CXX/Objects.hxx',
                                       'thirdparty/pycxx/CXX/Version.hxx',
                                       'thirdparty/pycxx/CXX/WrapPython.h',
                                       'thirdparty/pycxx/CXX/Python3/Config.hxx',
                                       'thirdparty/pycxx/CXX/Python3/CxxDebug.hxx',
                                       'thirdparty/pycxx/CXX/Python3/Exception.hxx',
                                       'thirdparty/pycxx/CXX/Python3/ExtensionModule.hxx',
                                       'thirdparty/pycxx/CXX/Python3/ExtensionOldType.hxx',
                                       'thirdparty/pycxx/CXX/Python3/ExtensionType.hxx',
                                       'thirdparty/pycxx/CXX/Python3/ExtensionTypeBase.hxx',
                                       'thirdparty/pycxx/CXX/Python3/Extensions.hxx',
                                       'thirdparty/pycxx/CXX/Python3/IndirectPythonInterface.hxx',
                                       'thirdparty/pycxx/CXX/Python3/Objects.hxx',
                                       'thirdparty/pycxx/CXX/Python3/PythonType.hxx',
                                       'thirdparty/pycxx/CXX/Python3/cxx_standard_exceptions.hxx'],
                              include_dirs=['thirdparty/pycxx', 'thirdparty/pycxx/Src', 'src'],
                              extra_compile_args=['-Wno-unused', '-fomit-frame-pointer',
                                                  '-DCDM_CF_PARSER_VERSION="' + version + '"',
                                                  '-ffast-math',
                                                  '-O2',
                                                  '-DPYCXX_PYTHON_2TO3'],
                              extra_link_args=extraLinkArgs,
                             )])
