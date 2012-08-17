# Only the neighborhoods supported by ruletree_algo are supported:
# a) vonNeumann: 4 neighbors: C,S,E,W,N,
# b) Moore: 8 neighbors: C,S,E,W,N,SE,SW,NE,NW
#
# This file contains two ways of building rule trees:
#
# 1) RuleTree Usage example:
#
# tree = RuleTree(14,4) # 14 states, 4 neighbors = von Neumann neighborhood
# tree.add_rule([[1],[1,2,3],[3],[0,1],[2]],7) # inputs: [C,S,E,W,N], output
# tree.write("Test.tree")
#
# 2) MakeRuleTreeFromTransitionFunction usage example:
#
# MakeRuleTreeFromTransitionFunction( 2, 4, lambda a:(a[0]+a[1]+a[2])%2, 'Parity.tree' )
#

import golly
import os

class RuleTree:
    '''
    Usage example:
    
    tree = RuleTree(14,4) # 14 states, 4 neighbors = von Neumann neighborhood
    tree.add_rule([[1],[1,2,3],[3],[0,1],[2]],7) # inputs: [C,S,E,W,N], output
    tree.write("Test.tree")
    
    For vonNeumann neighborhood, inputs are: C,S,E,W,N
    For Moore neighborhood, inputs are: C,S,E,W,N,SE,SW,NE,NW
    '''

    def __init__(self,numStates,numNeighbors):
    
        self.numParams = numNeighbors + 1 ;
        
        self.world = {} # dictionary mapping node tuples to node index (for speedy access by value)
        self.seq = [] # same node tuples but stored in a list (for access by index)
        # each node tuple is ( depth, index0, index1, .. index(numStates-1) )
        # where each index is an index into self.seq
        
        self.nodeSeq = 0
        self.curndd = -1
        self.numStates = numStates
        self.numNeighbors = numNeighbors
        
        self.cache = {}
        self.shrinksize = 100
        
        self._init_tree()
        
    def _init_tree(self):
        self.curndd = -1
        for i in range(self.numParams):
          node = tuple( [i+1] + [self.curndd]*self.numStates )
          self.curndd = self._getNode(node)

    def _getNode(self,node):
        if node in self.world:
            return self.world[node]
        else:
            iNewNode = self.nodeSeq
            self.nodeSeq += 1
            self.seq.append(node)
            self.world[node] = iNewNode
            return iNewNode
            
    def _add(self,inputs,output,nddr,at):
        if at == 0: # this is a leaf node
            if nddr<0:
                return output # return the output of the transition
            else:
                return nddr # return the node index
        if nddr in self.cache:
            return self.cache[nddr]
        # replace the node entry at each input with the index of the node from a recursive call to the next level down
        ### AKT: this code causes syntax error in Python 2.3:
        ### node = tuple( [at] + [ self._add(inputs,output,self.seq[nddr][i+1],at-1) if i in inputs[at-1] \
        ###                        else self.seq[nddr][i+1] for i in range(self.numStates) ] )
        temp = []
        for i in range(self.numStates):
            if i in inputs[at-1]:
                temp.append( self._add(inputs,output,self.seq[nddr][i+1],at-1) )
            else:
                temp.append( self.seq[nddr][i+1] )
        node = tuple( [at] + temp )
        r = self._getNode(node)
        self.cache[nddr] = r
        return r
            
    def _recreate(self,oseq,nddr,lev):
        if lev == 0:
            return nddr
        if nddr in self.cache:
            return self.cache[nddr]
        # each node entry is the node index retrieved from a recursive call to the next level down
        node = tuple( [lev] + [ self._recreate(oseq,oseq[nddr][i+1],lev-1) for i in range(self.numStates) ] )
        r = self._getNode(node)
        self.cache[nddr] = r
        return r

    def _shrink(self):
        self.world = {}
        oseq = self.seq
        self.seq = []
        self.cache = {}
        self.nodeSeq = 0 ;
        self.curndd = self._recreate(oseq, self.curndd, self.numParams)
        self.shrinksize = len(self.seq) * 2

    def add_rule(self,inputs,output):
        self.cache = {}
        self.curndd = self._add(inputs,output,self.curndd,self.numParams)
        if self.nodeSeq > self.shrinksize:
            self._shrink()
            
    def _setdefaults(self,nddr,off,at):
        if at == 0:
            if nddr<0:
                return off
            else:
                return nddr
        if nddr in self.cache:
            return self.cache[nddr]
        # each node entry is the node index retrieved from a recursive call to the next level down
        node = tuple( [at] + [ self._setdefaults(self.seq[nddr][i+1],i,at-1) for i in range(self.numStates) ] )
        node_index = self._getNode(node)
        self.cache[nddr] = node_index
        return node_index

    def _setDefaults(self):
        self.cache = {}
        self.curndd = self._setdefaults(self.curndd, -1, self.numParams)
      
    def write(self,filename):
        self._setDefaults()
        self._shrink()
        out = open(filename,'w')
        out.write("num_states=" + str(self.numStates)+'\n')
        out.write("num_neighbors=" + str(self.numNeighbors)+'\n')
        out.write("num_nodes=" + str(len(self.seq))+'\n')
        for rule in self.seq:
            out.write(' '.join(map(str,rule))+'\n')
        out.flush()
        out.close()

class MakeRuleTreeFromTransitionFunction:
    '''
    Usage example:

    MakeRuleTreeFromTransitionFunction( 2, 4, lambda a:(a[0]+a[1]+a[2])%2, 'Parity.tree' )
    
    For vonNeumann neighborhood, inputs are: N,W,E,S,C
    For Moore neighborhood, inputs are NW,NE,SW,SE,N,W,E,S,C
    '''
        
    def __init__(self,numStates,numNeighbors,f,filename):
        self.numParams = numNeighbors + 1 ;
        self.numStates = numStates
        self.numNeighbors = numNeighbors
        self.world = {}
        self.seq = []
        self.params = [0]*self.numParams
        self.nodeSeq = 0
        self.f = f
        self._recur(self.numParams)
        self._write(filename)

    def _getNode(self,node):
        if node in self.world:
            return self.world[node]
        else:
            iNewNode = self.nodeSeq
            self.nodeSeq += 1
            self.seq.append(node)
            self.world[node] = iNewNode
            return iNewNode
            
    def _recur(self,at):
        if at == 0:
            return self.f(self.params)
        node = tuple([at])
        for i in range(self.numStates):
            self.params[self.numParams-at] = i
            node += tuple( [self._recur(at-1)] )
        return self._getNode(node)

    def _write(self,filename):
        out = open(filename,'w')
        out.write("num_states=" + str(self.numStates)+'\n')
        out.write("num_neighbors=" + str(self.numNeighbors)+'\n')
        out.write("num_nodes=" + str(len(self.seq))+'\n')
        for rule in self.seq:
            out.write(' '.join(map(str,rule))+'\n')
        out.flush()
        out.close()

def ConvertRuleTableTransitionsToRuleTree(neighborhood,n_states,transitions,input_filename):
    '''Convert a set of vonNeumann or Moore transitions directly to a rule tree.'''
    rule_name = os.path.splitext(os.path.split(input_filename)[1])[0]
    remap = {
        "vonNeumann":[0,3,2,4,1], # CNESW->CSEWN
        "Moore":[0,5,3,7,1,4,6,2,8] # C,N,NE,E,SE,S,SW,W,NW -> C,S,E,W,N,SE,SW,NE,NW
    }
    numNeighbors = len(remap[neighborhood])-1
    tree = RuleTree(n_states,numNeighbors)
    for i,t in enumerate(transitions):
        golly.show("Building rule tree... ("+str(100*i/len(transitions))+"%)")
        tree.add_rule([ t[j] for j in remap[neighborhood] ],t[-1][0])
    tree.write(golly.getdir('rules')+rule_name+".tree" )
    return rule_name

