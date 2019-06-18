import sys
import simple

print( '--- module func ---' )
simple.mod_func_noargs()
simple.mod_func_varargs( 6, 7 )

simple.mod_func_keyword()
simple.mod_func_keyword( 4, 5 )
simple.mod_func_keyword( 4, 5, name=6, value=7 )

print( '--- old_style_class func ---' )
old_style_class = simple.old_style_class()
old_style_class.old_style_class_func_noargs()
old_style_class.old_style_class_func_varargs()
old_style_class.old_style_class_func_varargs( 4 )
old_style_class.old_style_class_func_keyword()
old_style_class.old_style_class_func_keyword( name=6, value=7 )
old_style_class.old_style_class_func_keyword( 4, 5 )
old_style_class.old_style_class_func_keyword( 4, 5, name=6, value=7 )

print( '--- Derived func ---' )
class Derived(simple.new_style_class):
    def __init__( self ):
        simple.new_style_class.__init__( self )

    def derived_func( self ):
        print( 'derived_func' )
        super( Derived, self ).func_noargs()

    def func_noargs( self ):
        print( 'derived func_noargs' )

d = Derived()
print( dir( d ) )
d.derived_func()
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

print( 'TEST: pass derived class to C++ world' )
result = simple.derived_class_test( d, 5, 9 )
print( 'derived_class_test result %r' % (result,) )

print( '--- new_style_class func ---' )
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
    print 'Error: did not raised RuntimeError'
    sys.exit( 1 )

except RuntimeError, e:
    print 'Raised %r' % (str(e),)

new_style_class = None
