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

"""Unit tests for the python control flow parser"""

from __future__ import print_function

import unittest
import os.path
import sys
import cdmcfparser
from cdmcfparser import (getControlFlowFromMemory,
                         getControlFlowFromFile, VERSION)


def formatFlow(s):
    """Reformats the control flow output"""
    result = ""
    shifts = []     # positions of opening '<'
    pos = 0         # symbol position in a line
    nextIsList = False

    def IsNextList(index, maxIndex, buf):
        if index == maxIndex:
            return False
        if buf[index + 1] == '<':
            return True
        if index < maxIndex - 1:
            if buf[index + 1] == '\n' and buf[index + 2] == '<':
                return True
        return False

    maxIndex = len(s) - 1
    for index in range(len(s)):
        sym = s[index]
        if sym == "\n":
            lastShift = shifts[-1]
            result += sym + lastShift * " "
            pos = lastShift
            if index < maxIndex:
                if s[index + 1] not in "<>":
                    result += " "
                    pos += 1
            continue
        if sym == "<":
            if not nextIsList:
                shifts.append(pos)
            else:
                nextIsList = False
            pos += 1
            result += sym
            continue
        if sym == ">":
            shift = shifts[-1]
            result += '\n'
            result += shift * " "
            pos = shift
            result += sym
            pos += 1
            if IsNextList(index, maxIndex, s):
                nextIsList = True
            else:
                del shifts[-1]
                nextIsList = False
            continue
        result += sym
        pos += 1
    return result


def files_equal(name1, name2):
    """Compares two files. Returns True if their content matches"""
    if not os.path.exists(name1):
        print("Cannot open " + name1, file=sys.stderr)
        return False

    if not os.path.exists(name2):
        print("Cannot open " + name2, file=sys.stderr)
        return False

    file1 = open(name1)
    file2 = open(name2)
    content1 = file1.read()
    content2 = file2.read()
    file1.close()
    file2.close()
    return content1.strip() == content2.strip()


class CDMControlFlowParserTest(unittest.TestCase):

    """Unit test class"""

    def setUp(self):
        """Initialisation"""
        self.dir = os.path.dirname(os.path.abspath(__file__)) + os.path.sep
        if not os.path.isdir(self.dir):
            raise Exception("Cannot find directory with tests. "
                            "Expected here: " + self.dir)

    def meat(self, pythonFile, errorMsg, expectedOK=True):
        """The test process meat"""
        controlFlow = getControlFlowFromFile(pythonFile)
        if controlFlow.isOK != expectedOK:
            self.fail("Error parsing the file " + pythonFile +
                      ". Option: directly from a file.")

        f = open(pythonFile)
        content = f.read()
        f.close()

        controlFlowMemory = getControlFlowFromMemory(content)
        if controlFlowMemory.isOK != expectedOK:
            self.fail("Error parsing the file " + pythonFile +
                      ". Option: from memory.")

        #if formatFlow(str(controlFlow)) != formatFlow(str(controlFlowMemory)):
        #    print('Memory:\n' + formatFlow(str(controlFlowMemory)))
        #    print('File:\n' + formatFlow(str(controlFlow)))
        #    self.fail("Control flow from memory differs the one from file")

        outFileName = pythonFile.replace(".py", ".out")
        outFile = open(outFileName, "w")
        outFile.write(formatFlow(str(controlFlow)) + "\n")
        outFile.close()

        okFileName = pythonFile.replace(".py", ".ok")
        if files_equal(outFileName, okFileName):
            return

        # Python 3 may serialize dictionary items in different order depending
        # on a particular run. This causes troubles with .ok file which
        # can have only one order. This is a quick and dirty workaround.
        index = 1
        while True:
            okFileNameOption = okFileName + str(index)
            if not os.path.exists(okFileNameOption):
                break       # Bad: no option matched
            if files_equal(outFileName, okFileNameOption):
                return      # Good: matched

            index += 1

        self.fail(errorMsg)

    def test_empty(self):
        """Test empty file"""
        self.meat(self.dir + "empty.py",
                  "empty file test failed")

    def test_bang(self):
        """Test file with a bang line"""
        self.meat(self.dir + "bang.py",
                  "bang line file test failed")

    def test_notbang(self):
        """Test that the second line is not a bang"""
        self.meat(self.dir + "notbang.py",
                  "second line must not be a bang failed")

    def test_import(self):
        """Tests imports"""
        self.meat(self.dir + "import.py",
                  "import test failed")

    def test_coding1(self):
        """Test coding 1"""
        self.meat(self.dir + "coding1.py",
                  "error retrieving coding")

    def test_coding2(self):
        """Test coding 2"""
        self.meat(self.dir + "coding2.py",
                  "error retrieving coding", False)

    def test_coding3(self):
        """Test coding 3"""
        self.meat(self.dir + "coding3.py",
                  "error retrieving coding")

    def test_cml1(self):
        """Test cml 1"""
        self.meat(self.dir + "cml1.py",
                  "error collecting cml")

    def test_cml2(self):
        """Test cml 2"""
        self.meat(self.dir + "cml2.py",
                  "error collecting cml with cml+ #2")

    def test_cml3(self):
        """Test cml 3"""
        self.meat(self.dir + "cml3.py",
                  "error collecting cml with cml+ #3")

    def test_cml4(self):
        """No record type"""
        self.meat(self.dir + "cml4.py",
                  "error collecting cml without a record type")

    def test_cml5(self):
        """No cml version"""
        self.meat(self.dir + "cml5.py",
                  "error collecting cml without a cml version")

    def test_cml6(self):
        """No cml value"""
        self.meat(self.dir + "cml6.py",
                  "error collecting cml without a value")

    def test_cml7(self):
        """No cml properties"""
        self.meat(self.dir + "cml7.py",
                  "error collecting cml without any properties")

    def test_docstring1(self):
        """Test docstring 1"""
        self.meat(self.dir + "docstring1.py",
                  "module docstring error - 1 line")

    def test_docstring2(self):
        """Test docstring 2"""
        self.meat(self.dir + "docstring2.py",
                  "module docstring error - 1 line")

    def test_docstring3(self):
        """Test docstring 3"""
        self.meat(self.dir + "docstring3.py",
                  "module docstring error - 2 lines")

    def test_docstring4(self):
        """Test docstring 4"""
        self.meat(self.dir + "docstring4.py",
                  "module docstring error - 1 line, joined")

    def test_docstring5(self):
        """Test docstring 5"""
        self.meat(self.dir + "docstring5.py",
                  "module docstring error - 1 line, joined")

    def test_docstring6(self):
        """Test docstring 6"""
        self.meat(self.dir + "docstring6.py",
                  "module docstring error - 2 lines, joined")

    def test_docstring7(self):
        """Test docstring 7"""
        self.meat(self.dir + "docstring7.py",
                  'module docstring error - multilined with """')

    def test_literal_stmt1(self):
        """Multiline string literal as a statement"""
        self.meat(self.dir + "stringliteral1.py",
                  'multiline string literal statement')

    def test_literal_stmt2(self):
        """Multiline string literal as a statement"""
        self.meat(self.dir + "stringliteral2.py",
                  'multiline string literal statement lead block')

    def test_literal_stmt3(self):
        """Multiline string literal as a statement"""
        self.meat(self.dir + "stringliteral3.py",
                  'multiline string literal statement tail block')

    def test_literal_stmt4(self):
        """Multiline string literal as a statement"""
        self.meat(self.dir + "stringliteral4.py",
                  'multiline string literal statement tail block')

    def test_literal_stmt5(self):
        """Multiline string literal as a statement"""
        self.meat(self.dir + "stringliteral5.py",
                  'multiline string literal statement tail block')

    def test_function_definitions(self):
        """Test function definitions"""
        self.meat(self.dir + "func_defs.py",
                  "function definition test failed")

    def test_function_comments(self):
        """Test function comments"""
        self.meat(self.dir + "func_comments.py",
                  "function comments test failed")

    def test_function_annotations(self):
        """Test function annotations"""
        self.meat(self.dir + "func_annotations.py",
                  "function annotations test failed")

#    def test_nested_func_definitions(self):
#        """Test nested functions definitions"""
#        self.meat(self.dir + "nested_funcs.py",
#                  "nested functions definitions test failed")

#    def test_globals(self):
#        """Test global variables"""
#        self.meat(self.dir + "globals.py",
#                  "global variables test failed")

    def test_class_definitions(self):
        """Test class definitions"""
        self.meat(self.dir + "class_defs.py",
                  "class definitions test failed")

#    def test_nested_classes(self):
#        """Test nested classes"""
#        self.meat(self.dir + "nested_classes.py",
#                  "nested classes test failed")

    def test_docstrings(self):
        """Test docstrings"""
        self.meat(self.dir + "docstring.py",
                  "docstring test failed")

    def test_docstring_comments(self):
        """Test docstrings"""
        self.meat(self.dir + "docstringcomments.py",
                  "docstring comments test failed")

#    def test_decorators(self):
#        """Test decorators"""
#        self.meat(self.dir + "decorators.py",
#                  "decorators test failed")

#    def test_static_members(self):
#        """Test class static members"""
#        self.meat(self.dir + "static_members.py",
#                  "class static members test failed")

#    def test_class_members(self):
#        """Test class members"""
#        self.meat(self.dir + "class_members.py",
#                  "class members test failed")

    def test_errors(self):
        """Test errors"""
        pythonFile = self.dir + "errors.py"
        info = getControlFlowFromFile(pythonFile)
        if info.isOK:
            self.fail("Expected parsing error for file " + pythonFile +
                      ". Option: directly from file.")

        outFileName = pythonFile.replace(".py", ".out")
        outFile = open(outFileName, "w")
        outFile.write(formatFlow(str(info)) + "\n")
        outFile.close()

        okFileName = pythonFile.replace(".py", ".ok")
        if not files_equal(outFileName, okFileName):
            self.fail("errors test failed")

    def test_wrong_indent(self):
        """Test wrong indent"""
        pythonFile = self.dir + "wrong_indent.py"
        info = getControlFlowFromFile(pythonFile)
        if info.isOK:
            self.fail("Expected parsing error for file " + pythonFile +
                      ". Option: directly from file.")

        outFileName = pythonFile.replace(".py", ".out")
        outFile = open(outFileName, "w")
        outFile.write(str(info) + "\n")
        outFile.close()

        okFileName = pythonFile.replace(".py", ".ok")
        if not files_equal(outFileName, okFileName):
            self.fail("wrong indent test failed")

    def test_one_comment(self):
        """Test for a file which consists of one comment line"""
        self.meat(self.dir + "one_comment.py",
                  "one comment line test failed")

    def test_comments_only(self):
        """Test for a file with no other lines except of comments"""
        self.meat(self.dir + "commentsonly.py",
                  "comments only with no other empty lines test failed")

    def test_noendofline(self):
        """Test for a file which has no EOL at the end"""
        self.meat(self.dir + "noendofline.py",
                  "No end of line at the end of the file test failed")

    def test_empty_brackets(self):
        """Test for empty brackets"""
        self.meat(self.dir + "empty_brackets.py",
                  "empty brackets test failed")

    def test_trailing_comments1(self):
        """Test trailing comments"""
        self.meat(self.dir + "trailingcomments.py",
                  "trailing comments test failed")


# Run the unit tests
if __name__ == '__main__':
    print("Testing control flow parser version: " + VERSION)
    print("Module location: " + cdmcfparser.__file__)
    unittest.main()
