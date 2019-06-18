import sys

def message( msg ):
    sys.stdout.write( msg )
    sys.stdout.write( '\n' )
    sys.stdout.flush()

message( 'Info: ---- %s ----' % (sys.argv[0],) )

message( 'TEST: import simple' )
import simple

message( 'TEST: call module functions' )
simple.func()
simple.func( 4, 5 )
simple.func( 4, 5, name=6, value=7 )

def callback_good( arg ):
    message( 'callback_good with %r' % (arg,) )
    return 'good result'

message( 'TEST: raise user defined exception' )
try:
    raise simple.SimpleError( 'Testing simple error' )

except simple.SimpleError as e:
    message( 'PASS SimpleError %s' % (e,) )

def callback_bad( arg ):
    message( 'callback_bad with %r' % (arg,) )
    raise ValueError( 'callback_bad error' )

def callback_raise_simple_error( arg ):
    message( 'callback_bad with %r' % (arg,) )
    raise simple.SimpleError( 'callback_raise_simple_error' )

message( 'TEST: call C++ with Python callback_good' )
answer = simple.func_with_callback( callback_good, 'fred' )
message( 'PASS callback_good returned %r' % (answer,) )

message( 'TEST: call C++ with Python callback_bad' )
try:
    answer = simple.func_with_callback( callback_bad, 'fred' )
    message( 'FAILED callback_bad %r' % (answer,) )

except Exception as e:
    message( 'PASS callback_bad: error %s' % (e,) )

message( 'TEST: call C++ with Python callback_raise_simple_error' )
try:
    answer = simple.func_with_callback( callback_raise_simple_error, 'fred' )
    message( 'FAIL callback_raise_simple_error returned %r' % (answer,) )

except simple.SimpleError as e:
    message( 'PASS callback_raise_simple_error: %s' % (e,) )

message( 'TEST: call C++ that will catch SimpleError' )
try:
    answer = simple.func_with_callback_catch_simple_error( callback_raise_simple_error, 'fred' )
    message( 'PASS func_with_callback_catch_simple_error returned %r' % (answer,) )

except simple.SimpleError as e:
    message( 'FAIL func_with_callback_catch_simple_error: %s' % (e,) )

message( 'TEST: raise SimpleError' )
try:
    raise simple.SimpleError( 'Hello!' )

except simple.SimpleError as e:
    message( 'PASS caught SimpleError - %s' % (e,) )

message( 'TEST: call old style class functions' )
old_style_class = simple.old_style_class()
old_style_class.old_style_class_func_noargs()
old_style_class.old_style_class_func_varargs()
old_style_class.old_style_class_func_varargs( 4 )
old_style_class.old_style_class_func_keyword()
old_style_class.old_style_class_func_keyword( name=6, value=7 )
old_style_class.old_style_class_func_keyword( 4, 5 )
old_style_class.old_style_class_func_keyword( 4, 5, name=6, value=7 )

message( 'TEST: Derived class functions' )
class Derived(simple.new_style_class):
    def __init__( self ):
        simple.new_style_class.__init__( self )

    def derived_func( self, arg ):
        message( 'derived_func' )
        super().func_noargs()

    def derived_func_bad( self, arg ):
        message( 'derived_func_bad' )
        raise ValueError( 'derived_func_bad value error' )

    def func_noargs( self ):
        message( 'derived func_noargs' )

d = Derived()
message( repr(dir( d )) )
d.derived_func( "arg" )
d.func_noargs()
d.func_varargs()
d.func_varargs( 4 )
d.func_keyword()
d.func_keyword( name=6, value=7 )
d.func_keyword( 4, 5 )
d.func_keyword( 4, 5, name=6, value=7 )

message( d.value )
d.value = "a string"
message( d.value )
d.new_var = 99

d.func_varargs_call_member( "derived_func" )
result = d.func_varargs_call_member( "derived_func_bad" )
message( 'derived_func_bad caught error: %r' % (result,) )

message( 'TEST: pass derived class to C++ world' )
result = simple.derived_class_test( d, 5, 9 )
message( 'derived_class_test result %r' % (result,) )

message( 'TEST: new_style_class functions' )
new_style_class = simple.new_style_class()
message( repr(dir( new_style_class )) )
new_style_class.func_noargs()
new_style_class.func_varargs()
new_style_class.func_varargs( 4 )
new_style_class.func_keyword()
new_style_class.func_keyword( name=6, value=7 )
new_style_class.func_keyword( 4, 5 )
new_style_class.func_keyword( 4, 5, name=6, value=7 )

try:
    new_style_class.func_noargs_raise_exception()
    message( 'Error: did not raised RuntimeError' )
    sys.exit( 1 )

except RuntimeError as e:
    message( 'Raised %r' % (str(e),) )


message( 'TEST: dereference new style class' )

new_style_class = None
