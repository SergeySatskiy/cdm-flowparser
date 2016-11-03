import os, sys
from glob import glob
from distutils.command.install import install
from distutils.core import setup

headers = (glob( os.path.join( "CXX","*.hxx" ) )
          +glob( os.path.join( "CXX","*.h" ) ))
sources = (glob( os.path.join( "Src", "*.cxx" ) )
          +glob( os.path.join( "Src", "*.c" ) ))


class my_install (install):

    def finalize_options (self):
        if not self.install_data or (len(self.install_data) < 8) :
            self.install_data = "$base/share/python$py_version_short"
        install.finalize_options (self)

    def run (self):
        self.distribution.data_files = [("CXX", sources)]
        self.distribution.headers = headers
        install.run (self)

# read the version from the master file CXX/Version.hxx
v_maj = None
v_min = None
v_pat = None
with open( 'CXX/Version.hxx', 'r' ) as f:
    for line in f:
        if line.startswith( '#define PYCXX_VERSION_' ):
            parts = line.strip().split()
            if parts[1] == 'PYCXX_VERSION_MAJOR':
                v_maj = parts[2]
            elif parts[1] == 'PYCXX_VERSION_MINOR':
                v_min = parts[2]
            elif parts[1] == 'PYCXX_VERSION_PATCH':
                v_pat = parts[2]

setup (name             = "CXX",
       version          = "%s.%s.%s" % (v_maj, v_min, v_pat),
       maintainer       = "Barry Scott",
       maintainer_email = "barry-scott@users.sourceforge.net",
       description      = "Facility for extending Python with C++",
       url              = "http://cxx.sourceforge.net",
       
       cmdclass         = {'install': my_install},
       packages         = ['CXX'],
       package_dir      = {'CXX': 'Lib'}
      )
