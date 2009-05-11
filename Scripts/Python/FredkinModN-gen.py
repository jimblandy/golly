import golly
import random
import time
n = int(golly.getstring(
'This script will write and select the rule "Fredkin-mod-n", for a given N.\n\n\
This rule is the Winograd extension of the Fredkin Replicator rule (the parity rule):\n\
  c,{s} -> sum(s)%N    (where N=2 for the original parity rule)\n\n\
If N is prime, this will result in any pattern being replicated at N^m timesteps (if\n\
the pattern is small enough).\n\n\
(N must lie within 2 and 255. Values above 13 or so result in very large .table\n\
files...)\n\nEnter N:','3','Enter N:'))
if n<2 or n>255:
   golly.exit('Values must lie between 2 and 255.')
golly.show('Working...')
# write a suitable colors file
g=open('../../Rules/Fredkin-mod-n.colors', 'w')
for i in range(0,n):
   g.write('color='+str(i+1)+' ' +str(random.randint(50,255)) + ' ' +
   str(random.randint(50,255))+' '+str(random.randint(50,255))+'\n')
g.close()
# write a rule table file
# (currently only for the von Neumann neighbourhood but works on many)
# (could be reduced by using rotate or permutation symmetry)
f=open('../../Rules/Fredkin-mod-n.table', 'w')
f.write("# Rule table written automatically by FredkinModN-gen.py\n#\n\
# Winograd's generalization of Fredkin's parity rule (B1357/S1357) to modulo-n:\n\
#   c,{s} -> sum(s)%n, where n=2 for original rule.\n#\n\
# Winograd, T. (1970) A simple algorithm for self-replication\n\
# A. I. Memo 197, Project MAC. MIT. http://hdl.handle.net/1721.1/5843\n\n")
f.write('n_states:'+str(n)+'\nneighborhood:vonNeumann\nsymmetries:none\n')
f.write('var a={'+','.join(str(s) for s in range(0,n))+'}\n')
# (there's probably a pythonic way to write these nested loops)
for a in range(0,n):
   for b in range(0,n):
       for c in range(0,n):
           for d in range(0,n):
               f.write('a,'+','.join(str(s) for s in
[a,b,c,d])+','+str((a+b+c+d)%n)+'\n')
f.close()
# now we can switch to the new rule
# problem: sometimes the file is still writing when we try to read it
golly.setrule('Fredkin-mod-n')
golly.show('Have written Rules/Fredkin-mod-n.table and .colors, and selected this rule.')
