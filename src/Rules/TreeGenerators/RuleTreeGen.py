class GenerateRuleTree:

    def __init__(self,numStates,numNeighbors,f):
    
        self.numParams = numNeighbors + 1 ;
        self.world = {}
        self.r = []
        self.params = [0]*self.numParams
        self.nodeSeq = 0
        self.numStates = numStates
        self.numNeighbors = numNeighbors
        self.f = f
        
        self.recur(self.numParams)
        self.writeRuleTree()

    def getNode(self,n):
        if n in self.world:
            return self.world[n]
        else:
            new_node = self.nodeSeq
            self.nodeSeq += 1
            self.r.append(n)
            self.world[n] = new_node
            return new_node

    def recur(self,at):
        if at == 0:
            return self.f(self.params)
        n = str(at)
        for i in range(self.numStates):
            self.params[self.numParams-at] = i
            n += " " + str(self.recur(at-1))
        return self.getNode(n)
      
    def writeRuleTree(self):
        print "num_states=" + str(self.numStates)
        print "num_neighbors=" + str(self.numNeighbors)
        print "num_nodes=" + str(len(self.r))
        for rule in self.r:
            print rule

# define your own transition function here:
def my_transition_function(a):
   # B3/S23
   n = a[0] + a[1] + a[2] + a[3] + a[4] + a[5] + a[6] + a[7]
   if n == 2 and not a[8] == 0:
      return 1
   if n == 3:
      return 1
   return 0

# call the rule tree generator with your chosen parameters
n_states = 2
n_neighbors = 8
GenerateRuleTree(n_states,n_neighbors,my_transition_function)
