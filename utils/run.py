#!/usr/bin/env python
# -*- coding: utf-8 -*-
#
# codimension - graphics python two-way code editor and analyzer
# Copyright (C) 2010-2016  Sergey Satskiy <sergey.satskiy@gmail.com>
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

" Convinience parser launcher "

import sys


def formatFlow( s ):
    " Reformats the control flow output "
    result = ""
    shifts = []     # positions of opening '<'
    pos = 0         # symbol position in a line
    nextIsList = False

    def IsNextList( index, maxIndex, buf ):
        if index == maxIndex:
            return False
        if buf[ index + 1 ] == '<':
            return True
        if index < maxIndex - 1:
            if buf[ index + 1 ] == '\n' and buf[ index + 2 ] == '<':
                return True
        return False

    maxIndex = len( s ) - 1
    for index in range( len( s ) ):
        sym = s[ index ]
        if sym == "\n":
            lastShift = shifts[ -1 ]
            result += sym + lastShift * " "
            pos = lastShift
            if index < maxIndex:
                if s[ index + 1 ] not in "<>":
                    result += " "
                    pos += 1
            continue
        if sym == "<":
            if nextIsList == False:
                shifts.append( pos )
            else:
                nextIsList = False
            pos += 1
            result += sym
            continue
        if sym == ">":
            shift = shifts[ -1 ]
            result += '\n'
            result += shift * " "
            pos = shift
            result += sym
            pos += 1
            if IsNextList( index, maxIndex, s ):
                nextIsList = True
            else:
                del shifts[ -1 ]
                nextIsList = False
            continue
        result += sym
        pos += 1
    return result





import cdmcf
from cdmcf import getControlFlowFromFile, VERSION

if len( sys.argv ) != 2:
    print( "Single file name is expected", file = sys.stderr )
    sys.exit( 1 )

print( "Running control flow parser version: " + VERSION )
print( "Module location: " + cdmcf.__file__ )

controlFlow = getControlFlowFromFile( sys.argv[ 1 ] )
print( formatFlow( str( controlFlow ) ) )
sys.exit( 0 )

