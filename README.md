# cdm-flowparser
cdm-flowparser project is a Python 2 extension module.
It takes a file with a python code (or a character buffer), parses it and
provides back a hierarchical representation of the code in terms of fragments.
Each fragment describes a portion of the input content:
a start point (line, column and absolute position) plus an end point (line, column and absolute position).

Comments are preserved too.

The parser is used in the Codimension Python IDE to generate a flowchart like diagrams for an arbitrary
Python code as the user types it. Basically a pause in typing is detected and the diagram is regenerated.

## Building from git clone

```shell
git clone https://github.com/SergeySatskiy/cdm-flowparser.git
cd cdm-flowparser
make
make check
make localinstall
```

## Visualizing parsed data

Suppose there is the following file ~/my-file.py with the following content:
```python
#!/usr/bin/python
import sys

# I like comments
a = 154
for x in xrange( a ):
    print "x = " + str( x )

sys.exit( 0 )
```

Then you can run a test utility:

```shell
~/cdm-flowparser/utils/run.py ~/my-file.py
```

The output will be the following:

```
Running control flow parser version: trunk
<ControlFlow [0:119] (1,1) (9,13)
 isOK: true
 Errors: n/a
 Warnings: n/a
 BangLine: [0:16] (1,1) (1,17)
 EncodingLine: None
 Docstring: None
 Suite: <Import [18:27] (2,1) (2,10)
         Body: [18:27] (2,1) (2,10)
         LeadingComment: None
         SideComment: None
         LeadingCMLComments: n/a
         SideCMLComments: n/a
         FromPart: None
         WhatPart: [25:27] (2,8) (2,10)
        >
        <CodeBlock [30:54] (4,1) (5,7)
         Body: [48:54] (5,1) (5,7)
         LeadingComment: <Comment [30:46] (4,1) (4,17)
                          Parts: <Fragment [30:46] (4,1) (4,17)
                                 >
                         >
         SideComment: None
         LeadingCMLComments: n/a
         SideCMLComments: n/a
        >
        <For [56:104] (6,1) (7,27)
         Body: [56:76] (6,1) (6,21)
         LeadingComment: None
         SideComment: None
         LeadingCMLComments: n/a
         SideCMLComments: n/a
         Iteration: [60:75] (6,5) (6,20)
         Suite: <CodeBlock [82:104] (7,5) (7,27)
                 Body: [82:104] (7,5) (7,27)
                 LeadingComment: None
                 SideComment: None
                 LeadingCMLComments: n/a
                 SideCMLComments: n/a
                >
         ElsePart: None
        >
        <SysExit [107:119] (9,1) (9,13)
         Body: [107:119] (9,1) (9,13)
         LeadingComment: None
         SideComment: None
         LeadingCMLComments: n/a
         SideCMLComments: n/a
         Argument: [117:117] (9,11) (9,11)
        >
>
```

## Usage

Basically the 'run.py' utility has the following essential lines

```python
from cdmcf import getControlFlowFromFile
controlFlow = getControlFlowFromFile( "my-file.py" )

# Serializes nicely the controlFlow object
# See the run.py for the details of how it is done
```

