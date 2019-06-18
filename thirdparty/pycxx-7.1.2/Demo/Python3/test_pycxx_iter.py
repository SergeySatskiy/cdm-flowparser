import sys

def message( msg ):
    sys.stdout.write( msg )
    sys.stdout.write( '\n' )
    sys.stdout.flush()

message( 'Info: ---- %s ----' % (sys.argv[0],) )

sys.path.insert( 0, 'pyds%d%d' % (sys.version_info[0], sys.version_info[1]) )

import pycxx_iter

it = pycxx_iter.IterT( 5, 7 )

for i in it:
    message( '%r %r' % (i, it) )

message( 'refcount of it: %d' % (sys.getrefcount( it ),) )
