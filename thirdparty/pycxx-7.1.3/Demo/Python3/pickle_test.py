import simple
import copyreg
import pickle
import pprint

class can_be_pickled(simple.new_style_class):
    def __init__( self, value ):
        super().__init__()
        self.value = value

    def __reduce__( self ):
        return (simple.new_style_class, (self.value,))

#n = can_be_pickled( 'QQQZZZQQQ' )
n = simple.new_style_class( 'QQQZZZQQQ' )

print( 'n.value:', n.value )

print( 'pickle.dumps' )
s = pickle.dumps( n )
print( 'dumps:', repr(s) )

print( 'pickle.loads' )
n2 = pickle.loads( s )
print( 'loads:', repr(n2) )
print( 'n2.value:', n2.value )
