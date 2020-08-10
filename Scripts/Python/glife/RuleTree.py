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
from tempfile import mkstemp
from shutil import move

# ------------------------------------------------------------------------------

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

# ------------------------------------------------------------------------------

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

# ------------------------------------------------------------------------------

def ReplaceTreeSection(rulepath, newtree):
    # replace @TREE section in existing .rule file with new tree data
    try:
        rulefile = open(rulepath,'r')
    except:
        golly.exit('Failed to open existing .rule file: '+rulepath)
    
    # create a temporary file for writing new rule info
    temphdl, temppath = mkstemp()
    tempfile = open(temppath,'w')
    
    skiplines = False
    for line in rulefile:
        if line.startswith('@TREE'):
            tempfile.write('@TREE\n\n')
            tempfile.write(newtree)
            skiplines = True
        elif skiplines and line.startswith('@'):
            tempfile.write('\n')
            skiplines = False
        if not skiplines:
            tempfile.write(line)
    
    # close files
    rulefile.close()
    tempfile.flush()
    tempfile.close()
    os.close(temphdl)
    
    # remove original .rule file and rename temporary file
    os.remove(rulepath)
    move(temppath, rulepath)

# ------------------------------------------------------------------------------

def GetColors(icon_pixels, wd, ht):
    colors = []
    multi_colored = False
    for row in range(ht):
        for col in range(wd):
            R,G,B = icon_pixels[row][col]
            if R != G or G != B:
                multi_colored = True    # not grayscale
            found = False
            index = 0
            for count, RGB in colors:
                if (R,G,B) == RGB:
                    found = True
                    break
                index += 1
            if found:
                colors[index][0] += 1
            else:
                colors.append([1, (R,G,B)])
    return colors, multi_colored

# ------------------------------------------------------------------------------

def hex2(i):
    # convert number from 0..255 into 2 hex digits
    hexdigit = "0123456789ABCDEF"
    result = hexdigit[i // 16]
    result += hexdigit[i % 16]
    return result

# ------------------------------------------------------------------------------

def CreateXPMIcons(colors, icon_pixels, iconsize, yoffset, xoffset, numicons, rulefile):
    # write out the XPM data for given icon size
    rulefile.write("\nXPM\n")
    cindex = "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    numcolors = len(colors)
    charsperpixel = 1
    if numcolors > 26:
        charsperpixel = 2   # AABA..PA, ABBB..PB, ... , APBP..PP
    
    rulefile.write("/* width height num_colors chars_per_pixel */\n")
    rulefile.write("\"" + str(iconsize) + " " + str(iconsize*numicons) + " " + \
                   str(numcolors) + " " + str(charsperpixel) + "\"\n")
    
    rulefile.write("/* colors */\n")
    n = 0
    for count, RGB in colors:
        R,G,B = RGB
        if R == 0 and G == 0 and B == 0:
            # nicer to show . or .. for black pixels
            rulefile.write("\".")
            if charsperpixel == 2: rulefile.write(".")
            rulefile.write(" c #000000\"\n")
        else:
            hexcolor = "#" + hex2(R) + hex2(G) + hex2(B)
            rulefile.write("\"")
            if charsperpixel == 1:
                rulefile.write(cindex[n])
            else:
                rulefile.write(cindex[n % 16] + cindex[n // 16])
            rulefile.write(" c " + hexcolor + "\"\n")
        n += 1
    
    for i in range(numicons):
        rulefile.write("/* icon for state " + str(i+1) + " */\n")
        for row in range(iconsize):
            rulefile.write("\"")
            for col in range(iconsize):
                R,G,B = icon_pixels[row + yoffset][col + xoffset*i]
                if R == 0 and G == 0 and B == 0:
                    # nicer to show . or .. for black pixels
                    rulefile.write(".")
                    if charsperpixel == 2: rulefile.write(".")
                else:
                    n = 0
                    thisRGB = (R,G,B)
                    for count, RGB in colors:
                        if thisRGB == RGB: break
                        n += 1
                    if charsperpixel == 1:
                        rulefile.write(cindex[n])
                    else:
                        rulefile.write(cindex[n % 16] + cindex[n // 16])
            rulefile.write("\"\n")

# ------------------------------------------------------------------------------

def ConvertTreeToRule(rule_name, total_states, icon_pixels):
    '''
    Convert rule_name.tree to rule_name.rule and delete the .tree file.
    
    If rule_name.colors exists then use it to create an @COLORS section
    and delete the .colors file.
    
    If icon_pixels is supplied then add an @ICONS section.
    Format of icon_pixels (in this example there are 4 icons at each size):
    
    ---------------------------------------------------------
    |             |             |             |             |
    |             |             |             |             |
    |    31x31    |    31x31    |    31x31    |    31x31    |
    |             |             |             |             |
    |             |             |             |             |
    ---------------------------------------------------------
    |       |.....|       |.....|       |.....|       |.....|
    | 15x15 |.....| 15x15 |.....| 15x15 |.....| 15x15 |.....|
    |       |.....|       |.....|       |.....|       |.....|
    ---------------------------------------------------------
    |7x7|.........|7x7|.........|7x7|.........|7x7|.........|
    ---------------------------------------------------------
    
    The top layer of 31x31 icons is optional -- if not supplied (ie. the
    height is 22) then there are no gaps between the 15x15 icons.
    '''
    rulepath = golly.getdir('rules')+rule_name+'.rule'
    treepath = golly.getdir('rules')+rule_name+'.tree'
    colorspath = golly.getdir('rules')+rule_name+'.colors'

    # get contents of .tree file
    try:
        treefile = open(treepath,'r')
        treedata = treefile.read()
        treefile.close()
    except:
        golly.exit('Failed to open .tree file: '+treepath)
    
    # if the .rule file already exists then only replace the @TREE section
    # so we don't clobber any other info added by the user
    if os.path.isfile(rulepath):
        ReplaceTreeSection(rulepath, treedata)
        os.remove(treepath)
        if os.path.isfile(colorspath): os.remove(colorspath)
        return
    
    # create a new .rule file
    rulefile = open(rulepath,'w')
    rulefile.write('@RULE '+rule_name+'\n\n')
    rulefile.write('@TREE\n\n')
    
    # append contents of .tree file, then delete that file
    rulefile.write(treedata)
    os.remove(treepath)
    
    # if .colors file exists then append @COLORS section and delete file
    if os.path.isfile(colorspath):
        colorsfile = open(colorspath,'r')
        rulefile.write('\n@COLORS\n\n')
        for line in colorsfile:
            if line.startswith('color') or line.startswith('gradient'):
                # strip off everything before 1st digit
                line = line.lstrip('colorgadient= \t')
            rulefile.write(line)
        colorsfile.close()
        os.remove(colorspath)
    
    # if icon pixels are supplied then append @ICONS section
    if len(icon_pixels) > 0:
        wd = len(icon_pixels[0])
        ht = len(icon_pixels)
        iconsize = 15                   # size of icons in top row
        if ht > 22: iconsize = 31       # 31x31 icons are present
        numicons = wd // iconsize
        
        # get colors used in all icons (we assume each icon size uses the same set of colors)
        colors, multi_colored = GetColors(icon_pixels, wd, ht)
        if len(colors) > 256:
            golly.warn('Icons use more than 256 colors!')
            rulefile.flush()
            rulefile.close()
            return
        if multi_colored:
            # create @COLORS section using color info in icon_pixels (not grayscale)
            rulefile.write('\n@COLORS\n\n')
            if numicons >= total_states:
                # extra icon is present so use top right pixel to set the color of state 0
                R,G,B = icon_pixels[0][wd-1]
                rulefile.write('0 ' + str(R) + ' ' + str(G) + ' ' + str(B) + '\n')
                numicons -= 1
            # set colors for each live state to the average of the non-black pixels
            # in each icon on top row (note we've skipped the extra icon detected above)
            for i in range(numicons):
                nbcount = 0
                totalR = 0
                totalG = 0
                totalB = 0
                for row in range(iconsize):
                    for col in range(iconsize):
                        R,G,B = icon_pixels[row][col + i*iconsize]
                        if R > 0 or G > 0 or B > 0:
                            nbcount += 1
                            totalR += R
                            totalG += G
                            totalB += B
                if nbcount > 0:
                    rulefile.write(str(i+1) + ' ' + str(totalR // nbcount) + ' ' \
                                                  + str(totalG // nbcount) + ' ' \
                                                  + str(totalB // nbcount) + '\n')
                else:
                    # avoid div by zero
                    rulefile.write(str(i+1) + ' 0 0 0\n')
        
        # create @ICONS section using (r,g,b) triples in icon_pixels[row][col]
        rulefile.write('\n@ICONS\n')
        if ht > 22:
            # top row of icons is 31x31
            CreateXPMIcons(colors, icon_pixels, 31,  0, 31, numicons, rulefile)
            CreateXPMIcons(colors, icon_pixels, 15, 31, 31, numicons, rulefile)
            CreateXPMIcons(colors, icon_pixels,  7, 46, 31, numicons, rulefile)
        else:
            # top row of icons is 15x15
            CreateXPMIcons(colors, icon_pixels, 15,  0, 15, numicons, rulefile)
            CreateXPMIcons(colors, icon_pixels,  7, 15, 15, numicons, rulefile)
    
    rulefile.flush()
    rulefile.close()

# ------------------------------------------------------------------------------

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
    # use rule_name.tree to create rule_name.rule (no icons)
    ConvertTreeToRule(rule_name, n_states, [])
    return rule_name
