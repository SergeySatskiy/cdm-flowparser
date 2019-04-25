## cdm-flowparser [![Build Status](https://travis-ci.org/SergeySatskiy/cdm-flowparser.svg?branch=master)](https://travis-ci.org/SergeySatskiy/cdm-flowparser)
[cdm-flowparser project](https://github.com/SergeySatskiy/cdm-flowparser)
is a Python 3 (Python 2 support is limited) extension module.
The module provided functions can takes a file with a python code or a character buffer,
parse it and provide back a hierarchical representation of the code in terms of fragments.
Each fragment describes a portion of the input:
a start point (line, column and absolute position) plus an end point
(line, column and absolute position).

Comments are preserved too.

The module is used in the [Codimension Python IDE](http://codimension.org) to
generate a flowchart-like diagrams for an arbitrary Python code as the user
types it. Basically the IDE detects a pause in typing and regenerates the diagram.

## Python 3 Installation and Building
The [master branch](https://github.com/SergeySatskiy/cdm-flowparser) contains code for Python 3 (3.5/3.6/3.7 grammar is covered).

The module can be installed using pip:

```shell
pip install cdmcfparser
```

You can also retrieve the full source code which in addition has some utilities.
In order to do that you can follow these steps:

```shell
git clone https://github.com/SergeySatskiy/cdm-flowparser.git
cd cdm-flowparser
make
make check
make localinstall
```


## Python 3: Visualizing Parsed Data
Suppose there is ~/my-file.py file with the following content:
```python
#!/bin/env python
import sys

# I like comments
a = 154
for x in range(a):
    print("x = " + str(x))

sys.exit(0)
```

Then you can run a test utility (if you have a local [repository](https://github.com/SergeySatskiy/cdm-flowparser) clone):

```shell
~/cdm-flowparser/utils/run.py ~/my-file.py
```

The output will be the following:

```
Running control flow parser version: trunk
Module location: /home/swift/cdm-flowparser/cdmcfparser.cpython-35m-x86_64-linux-gnu.so
<ControlFlow [0:113] (1,1) (9,11)
 Body: [18:113] (2,1) (9,11)
 LeadingComment: None
 SideComment: None
 LeadingCMLComments: n/a
 SideCMLComments: n/a
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
        <For [56:100] (6,1) (7,26)
         Body: [56:73] (6,1) (6,18)
         LeadingComment: None
         SideComment: None
         LeadingCMLComments: n/a
         SideCMLComments: n/a
         Async: None
         For: [56:58] (6,1) (6,3)
         Iteration: [60:72] (6,5) (6,17)
         Suite: <CodeBlock [79:100] (7,5) (7,26)
                 Body: [79:100] (7,5) (7,26)
                 LeadingComment: None
                 SideComment: None
                 LeadingCMLComments: n/a
                 SideCMLComments: n/a
                >
         ElsePart: None
        >
        <CodeBlock [103:113] (9,1) (9,11)
         Body: [103:113] (9,1) (9,11)
         LeadingComment: None
         SideComment: None
         LeadingCMLComments: n/a
         SideCMLComments: n/a
        >
>
```



## Python 2 Installation and Building
**Attention:** Python 2 version is not supported anymore.
There will be no more Python 2 releases.

The latest Python 2 release is 1.0.1. Both pre-built modules and
source code are available in the releases area on github:
[latest Python 2 release 1.0.1](https://github.com/SergeySatskiy/cdm-flowparser/releases/tag/v1.0.1).

To build a Python 2 module from sources please follow these steps:

```shell
cd
wget https://github.com/SergeySatskiy/cdm-flowparser/archive/v1.0.1.tar.gz
gunzip v1.0.1.tar.gz
tar -xf v1.0.1.tar
cd cdm-flowparser-1.0.1/
make
make localinstall
make check
```


## Python 2: Visualizing Parsed Data
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

## Under the Hood

Basically the 'run.py' utility has the following essential lines for the example above:

```python
from cdmcfparser import getControlFlowFromFile
controlFlow = getControlFlowFromFile("my-file.py")

# Serializes nicely the controlFlow object
# See the run.py for the details of how it is done
```

The run.py is available in a local clone at ~/cdm-flowparser/utils/run.py or
you can see the source code [online](https://github.com/SergeySatskiy/cdm-flowparser/blob/master/utils/run.py)


## Essential Links
- [Codimension Python IDE](http://codimension.org) home page
- [latest Python 2 release 1.0.1](https://github.com/SergeySatskiy/cdm-flowparser/releases/tag/v1.0.1)
- [Python 3 Pypi package](https://pypi.python.org/pypi?name=cdmcfparser&:action=display) page
- [Visualization Technology](http://codimension.org/documentation/visualization-technology/python-code-visualization.html)
