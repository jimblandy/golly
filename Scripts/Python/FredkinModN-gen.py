import golly
import random
import time

n = int(golly.getstring(
'''This script will write and select the rule "Fredkin-mod-n", for a given N.

This rule is the Winograd extension of the Fredkin Replicator rule (the parity rule):
  c,{s} -> sum(s)%N    (where N=2 for the original parity rule)

If N is prime, this will result in any pattern being replicated at N^m timesteps
(if the pattern is small enough).

(N must lie within 2 and 255. Values above 13 or so result in very large .table
files...)

Enter N:''', '3', 'Enter N:'))
if n<2 or n>255:
   golly.exit('Values must lie between 2 and 255.')

# write a suitable colors file
golly.show('Working...')
f=open(golly.getdir('rules')+'Fredkin-mod-n.colors', 'w')
for i in xrange(n):
   f.write('color='+str(i+1)+' '+str(random.randint(50,255))+' '+
                                 str(random.randint(50,255))+' '+
                                 str(random.randint(50,255))+'\n')
f.close()

# write a rule table file in user's rules directory
# (currently only for the von Neumann neighbourhood but works on many)
# (could be reduced by using rotate or permutation symmetry)
f=open(golly.getdir('rules')+'Fredkin-mod-n.table', 'w')
f.write('''# Rule table written automatically by FredkinModN-gen.py
#
# Winograd's generalization of Fredkin's parity rule (B1357/S1357) to modulo-n:
#   c,{s} -> sum(s)%n, where n=2 for original rule.
#
# Winograd, T. (1970) A simple algorithm for self-replication
# A. I. Memo 197, Project MAC. MIT. http://hdl.handle.net/1721.1/5843

''')
f.write('n_states:'+str(n)+'\nneighborhood:vonNeumann\nsymmetries:none\n')
f.write('var a={'+','.join(str(s) for s in xrange(n))+'}\n')
for a in xrange(n):
   for b in xrange(n):
       for c in xrange(n):
           for d in xrange(n):
               f.write('a,'+','.join(str(s) for s in [a,b,c,d])+
                       ','+str((a+b+c+d)%n)+'\n')
f.close()

# now we can switch to the new rule
# problem: sometimes the file is still writing when we try to read it
time.sleep(0.1)            # perhaps this will avoid the problem?
golly.setrule('b1/s1')     # force a rule change
golly.setrule('Fredkin-mod-n')
golly.show('Created Fredkin-mod-n.table and .colors, and selected this rule.')
