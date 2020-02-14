#!/usr/bin/env python
# -*- coding: utf-8 -*-
#
# codimension - graphics python two-way code editor and analyzer
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

"""Speed test for the Codimension control flow parser"""

import os, os.path, sys
import datetime
import cdmcfparser
from cdmcfparser import getControlFlowFromFile, VERSION
import pdb
import gc
from guppy import hpy



def cdmcfparserTest(fName):
    """Loop for the codimension parser"""
    tempObj = getControlFlowFromFile(fName)
    tempObj = None


fullPath = os.path.abspath(__file__)

print("Speed test measures the time required for "
      "cdmcfparser to parse python files.")
print("Parser version: " + VERSION)
print("Module location: " + cdmcfparser.__file__)
print("Full path: " + fullPath)


h = hpy()
h.setref()

for x in range(1000):
    cdmcfparserTest(fullPath)

gc.collect()
heap = h.heap()
unreachable = h.heapu()

pdb.set_trace()

