# Generates a rule tree using a given Python transition function
# passed in via the clipboard.

# Here's an example function for Conway's Life
# (copy all the lines between the triple quotes):
'''
# B3/S23:
name = "LifeTest"
n_states = 2
n_neighbors = 8
# order for 8 neighbors is NW, NE, SW, SE, N, W, E, S, C
def transition_function(a):
    n = a[0]+a[1]+a[2]+a[3]+a[4]+a[5]+a[6]+a[7]
    if n==2 and a[8]==1:
        return 1
    if n==3:
        return 1
    return 0
'''

# Another example, but using the vonNeumann neighborhood:
'''
name = "ParityNWE"
n_states = 5
n_neighbors = 4
# order for 4 neighbors is N, W, E, S, C
def transition_function ( s ) :
    return ( s[0] + s[1] + s[2] ) % 5
'''

import golly
from glife.RuleTree import *

# exec() only works if all lines end with LF, so we need to convert
# any Win line endings (CR+LF) or Mac line endings (CR) to LF
CR = chr(13)
LF = chr(10)
try:

    exec(golly.getclipstr().replace(CR+LF,LF).replace(CR,LF))
    MakeRuleTreeFromTransitionFunction( n_states, n_neighbors, transition_function,
                                        golly.getdir("rules")+name+".tree" )
    
    # use name.tree to create name.rule (with no icons);
    # note that if name.rule already exists then we only replace the info in
    # the @TREE section to avoid clobbering any other info added by the user
    ConvertTreeToRule(name, n_states, [])
    
    golly.setalgo("RuleLoader")
    golly.setrule(name)
    golly.show("Created "+golly.getdir("rules")+name+".rule and switched to that rule.")

except:
    import sys
    import traceback
    exception, msg, tb = sys.exc_info()
    golly.warn(\
 '''To use this script, copy a Python format rule definition into the clipboard, e.g.:

 name = "ParityNWE"
 n_states = 5
 n_neighbors = 4
 # order for 4 neighbors is N, W, E, S, C
 def transition_function ( s ) :
       return ( s[0] + s[1] + s[2] ) % 5

 For more examples, see the script file (right-click on it).
 ____________________________________________________

 A problem was encountered with the supplied rule:

 '''+ '\n'.join(traceback.format_exception(exception, msg, tb)))
    golly.exit()
