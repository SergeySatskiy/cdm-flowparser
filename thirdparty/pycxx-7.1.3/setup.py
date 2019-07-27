import os, sys
from glob import glob
from distutils.command.install import install
from distutils.command.install_headers import install_headers
from distutils.core import setup

# either "Python2" or "Python3"
python_ver = "Python" + sys.version[0]

headers = [
    (None, glob( os.path.join( "CXX", "*.hxx" ) ) + glob( os.path.join( "CXX", "*.h" ) )),
    (python_ver, glob( os.path.join( "CXX", python_ver, "*.hxx" ) ))
    ]

sources = [
    ("CXX", glob( os.path.join( "Src", "*.cxx" ) ) + glob( os.path.join( "Src", "*.c" ) )),
    (os.path.join( "CXX", python_ver ), glob( os.path.join( "Src", python_ver, "*" ) ))
    ]

class my_install(install):
    def finalize_options( self ):
        if not self.install_data or (len(self.install_data) < 8):
            self.install_data = "$base/share/python$py_version_short"
        install.finalize_options (self)

    def run (self):
        self.distribution.data_files = sources
        self.distribution.headers = headers
        install.run( self )

class my_install_headers(install_headers):
    def run( self ):
        if not self.distribution.headers:
            return

        for subdir, headers in self.distribution.headers:
            try:
                dir = os.path.join( self.install_dir, subdir )
            except:
                dir = self.install_dir
            self.mkpath( dir )
            for header in headers:
                (out, _) = self.copy_file( header, dir )
                self.outfiles.append( out )


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

setup( name             = "CXX",
       version          = "%s.%s.%s" % (v_maj, v_min, v_pat),
       maintainer       = "Barry Scott",
       maintainer_email = "barry-scott@users.sourceforge.net",
       description      = "Facility for extending Python with C++",
       url              = "http://cxx.sourceforge.net",

       cmdclass         = {'install': my_install,
                           'install_headers': my_install_headers},
       packages         = ['CXX'],
       package_dir      = {'CXX': 'Lib'}
    )
