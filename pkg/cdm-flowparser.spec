%define name cdm-flowparser
%define release 1
%define version %{getenv:version}

Summary: Parse python code to hierarchical representation in terms of fragments.
Name: %{name}
Version: %{version}
Release: %{release}%{?dist}
License: GPLv3+
Group: Development/Libraries
URL: https://github.com/SergeySatskiy/codimension

Requires: python

BuildRequires: python-devel
BuildRequires: gcc-c++
Source: https://github.com/SergeySatskiy/codimension/releases/%{name}-%{version}.tar.gz

%description
cdm-flowparser takes a file with a python code (or a character buffer), parses it and
provides back a hierarchical representation of the code in terms of fragments.
Each fragment describes a portion of the input content:
a start point (line, column and absolute position) plus an end point (line, column and absolute position).

Comments are preserved too.

The parser is used in the Codimension Python IDE to generate a flowchart like diagrams for an arbitrary
Python code as the user types it. Basically a pause in typing is detected and the diagram is regenerated.

%prep
%setup -q -n %{name}-%{version}

%build
make

%install
python setup.py install --root=${RPM_BUILD_ROOT} --record=INSTALLED_FILES

%check
make check

%files -f INSTALLED_FILES
%doc AUTHORS LICENSE ChangeLog README.md docs/cml.txt
#%{python_sitearch}/cdmbriefparser.pyo

%changelog
* Sat Jan 16 2016 Sergey Fukanchik <fukanchik@gmail.com> - 2.0.0
- Initial version of the package.

