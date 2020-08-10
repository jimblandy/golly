import golly
import random
import time
from glife.EmulateHexagonal import *
from glife.EmulateTriangular import *
from glife.RuleTree import *

# AKT: itertools.product is new in Python 2.6
try:
    from itertools import product
except ImportError:
    # see http://docs.python.org/library/itertools.html#itertools.product
    def product(*args, **kwds):
        pools = list(map(tuple, args)) * kwds.get('repeat', 1)
        result = [[]]
        for pool in pools:
            result = [x+[y] for x in result for y in pool]
        for prod in result:
            yield tuple(prod)

spec = golly.getstring(
'''This script will write and select the rule "Fredkin-mod-n", for a given N.

This rule is the Winograd extension of the Fredkin Replicator rule (the parity rule):
  c,{s} -> sum(s)%N    (where N=2 for the original parity rule)

If N is prime, this will result in any pattern being replicated at N^m timesteps
(if the pattern is small enough that it doesn't overlap its copies).

Specify the neighborhood (N = von Neumann, M = Moore, T = triangular von Neumann,
H = hexagonal, TM = triangular Moore) and the value of N (2-255). e.g. H2, N7

(Larger values of N can take a long time.)
''', 'N3', 'Enter specification:')

# work out what the string meant
nhood = ''
nhoods = {'N':'vonNeumann','TM':'triangularMoore','M':'Moore',
          'T':'triangularVonNeumann','H':'hexagonal'}
for nh in list(nhoods.keys()):
    if nh in spec:
        nhood = nhoods[nh]
        n = int(spec.replace(nh,''))
        break
if nhood=='':
    golly.exit('Unsupported string: '+spec)
if n<2 or n>255:
    golly.exit('Value of N must lie between 2 and 255.')

# assemble the transitions
nbors = {'vonNeumann':4,'Moore':8,'hexagonal':6,'triangularVonNeumann':3,
         'triangularMoore':12}
transitions = []
for sl in product(list(range(n)),repeat=nbors[nhood]):
    transitions += [ [list(range(n))] + [[s] for s in sl] + [[sum(sl)%n]] ]
rule_name = 'Fredkin_mod'+str(n)+'_'+nhood

Converters = {
    "vonNeumann":ConvertRuleTableTransitionsToRuleTree,
    "Moore":ConvertRuleTableTransitionsToRuleTree,
    "triangularVonNeumann":EmulateTriangular,
    "triangularMoore":EmulateTriangular,
    "hexagonal":EmulateHexagonal,
}

golly.show("Building rule...")
rule_name = Converters[nhood]( nhood,
                               n,
                               transitions,
                               rule_name+'.tree' )
golly.setrule(rule_name)
golly.show('Created '+rule_name+'.rule and selected that rule.')
