print( 'TEST: import simple' )
import simple

print( 'TEST: call module functions' )
simple.func()
simple.func( 4, 5 )
simple.func( 4, 5, name=6, value=7 )

def callback_good( arg ):
    print( 'callback_good with %r' % (arg,) )
    return 'good result'

print( 'TEST: raise user defined exception' )
try:
    raise simple.SimpleError( 'Testing simple error' )

except simple.SimpleError as e:
    print( 'PASS SimpleError %s' % (e,) )


def callback_bad( arg ):
    print( 'callback_bad with %r' % (arg,) )
    raise ValueError( 'callback_bad error' )

def callback_raise_simple_error( arg ):
    print( 'callback_bad with %r' % (arg,) )
    raise simple.SimpleError( 'callback_raise_simple_error' )

print( 'TEST: call C++ with Python callback_good' )
answer = simple.func_with_callback( callback_good, 'fred' )
print( 'PASS callback_good returned %r' % (answer,) )

print( 'TEST: call C++ with Python callback_bad' )
try:
    answer = simple.func_with_callback( callback_bad, 'fred' )
    print( 'FAILED callback_bad %r' % (answer,) )

except Exception as e:
    print( 'PASS callback_bad: error %s' % (e,) )

print( 'TEST: call C++ with Python callback_raise_simple_error' )
try:
    answer = simple.func_with_callback( callback_raise_simple_error, 'fred' )
    print( 'FAIL callback_raise_simple_error returned %r' % (answer,) )

except simple.SimpleError as e:
    print( 'PASS callback_raise_simple_error: %s' % (e,) )

print( 'TEST: call C++ that will catch SimpleError' )
try:
    answer = simple.func_with_callback_catch_simple_error( callback_raise_simple_error, 'fred' )
    print( 'PASS func_with_callback_catch_simple_error returned %r' % (answer,) )

except simple.SimpleError as e:
    print( 'FAIL func_with_callback_catch_simple_error: %s' % (e,) )

print( 'TEST: raise SimpleError' )
try:
    raise simple.SimpleError( 'Hello!' )

except simple.SimpleError as e:
    print( 'PASS caught SimpleError - %s' % (e,) )

print( 'TEST: call old style class functions' )
old_style_class = simple.old_style_class()
old_style_class.old_style_class_func_noargs()
old_style_class.old_style_class_func_varargs()
old_style_class.old_style_class_func_varargs( 4 )
old_style_class.old_style_class_func_keyword()
old_style_class.old_style_class_func_keyword( name=6, value=7 )
old_style_class.old_style_class_func_keyword( 4, 5 )
old_style_class.old_style_class_func_keyword( 4, 5, name=6, value=7 )

print( 'TEST: Derived class functions' )
class Derived(simple.new_style_class):
    def __init__( self ):
        simple.new_style_class.__init__( self )

    def derived_func( self, arg ):
        print( 'derived_func' )
        super().func_noargs()

    def derived_func_bad( self, arg ):
        print( 'derived_func_bad' )
        raise ValueError( 'derived_func_bad value error' )

    def func_noargs( self ):
        print( 'derived func_noargs' )

d = Derived()
print( dir( d ) )
d.derived_func( "arg" )
d.func_noargs()
d.func_varargs()
d.func_varargs( 4 )
d.func_keyword()
d.func_keyword( name=6, value=7 )
d.func_keyword( 4, 5 )
d.func_keyword( 4, 5, name=6, value=7 )

print( d.value )
d.value = "a string"
print( d.value )
d.new_var = 99

d.func_varargs_call_member( "derived_func" )
result = d.func_varargs_call_member( "derived_func_bad" )
print( 'derived_func_bad caught error: %r' % (result,) )

print( 'TEST: pass derived class to C++ world' )
result = simple.derived_class_test( d, 5, 9 )
print( 'derived_class_test result %r' % (result,) )

print( 'TEST: new_style_class functions' )
new_style_class = simple.new_style_class()
print( dir( new_style_class ) )
new_style_class.func_noargs()
new_style_class.func_varargs()
new_style_class.func_varargs( 4 )
new_style_class.func_keyword()
new_style_class.func_keyword( name=6, value=7 )
new_style_class.func_keyword( 4, 5 )
new_style_class.func_keyword( 4, 5, name=6, value=7 )

try:
    new_style_class.func_noargs_raise_exception()
    print( 'Error: did not raised RuntimeError' )
    sys.exit( 1 )

except RuntimeError as e:
    print( 'Raised %r' % (str(e),) )


print( 'TEST: dereference new style class' )

new_style_class = None
