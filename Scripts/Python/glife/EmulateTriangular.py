# Inspired by Andrew Trevorrow's work on TriLife.py

try:
    set
except NameError:
    # use sets module if Python version is < 2.4
    from sets import Set as set

import golly
import os
from glife.RuleTree import *

# We support two emulation methods:
# 1) square-splitting: each square holds two triangles: one up, one down
# 2) checkerboard: each square holds one triangle, either up or down
#
# The first method has better appearance but requires N*N states. The second requires only 2N
# states but user must respect the checkerboard when drawing. For triangularMoore emulation, only
# the first is possible (since Golly only natively supports vonNeumann and Moore). Thus we choose
# the first where possible, only using the second for triangularVonNeumann when N>16.
#
# 1) Square-splitting emulation: (following Andrew Trevorrow's TriLife.py)
#
#  triangularVonNeumann -> vonNeumann:
#
#      lower         upper
#       +--+          +--+
#       |\ |          |\ |
#       | \|          |2\|          (any rotation would work too)
#    +--+--+--+    +--+--+--+
#    |\3|\1|\ |    |\ |\0|\ |
#    | \|0\| \|    | \|1\|3\|
#    +--+--+--+    +--+--+--+
#       |\2|          |\ |
#       | \|          | \|
#       +--+          +--+
#
# triangularMoore -> Moore:
#
#      lower         upper
#    +--+--+--+    +--+--+--+
#    |\B|\ |\ |    |\6|\7|\ |       where A=10, B=11, C=12
#    |A\|C\| \|    |5\|2\|8\|
#    +--+--+--+    +--+--+--+
#    |\3|\1|\ |    |\4|\0|\9|
#    |9\|0\|4\|    | \|1\|3\|
#    +--+--+--+    +--+--+--+
#    |\8|\2|\5|    |\ |\C|\A|
#    | \|7\|6\|    | \| \|B\|
#    +--+--+--+    +--+--+--+
#
# 2) Checkerboard emulation: (drawn using A and V to represent triangles in two orientations)
#
#    A V A V A V A V       where each A has 3 V neighbors:      V A V
#    V A V A V A V A                                              V
#    A V A V A V A V
#    V A V A V A V A       and each V has 3 A neighbors:        A
#    A V A V A V A V                                          A V A
#    V A V A V A V A

def TriangularTransitionsToRuleTree_SplittingMethod(neighborhood,n_states,transitions_list,rule_name):

    # each square cell is j*N+i where i is the lower triangle, j is the upper triangle
    # each i,j in (0,N]
    # (lower and upper are lists)
    def encode(lower,upper):
        return [ up*n_states+low for up in upper for low in lower ]

    # what neighbors of the lower triangle overlap neighbors of the upper triangle?
    lower2upper = {
        "triangularVonNeumann": [(0,1),(1,0)],
        "triangularMoore": [(0,1),(1,0),(2,12),(3,4),(4,3),(5,10),(6,11),(10,5),(11,6),(12,2)],
    }
    numNeighbors = { "triangularVonNeumann":4, "triangularMoore":8 }

    # convert transitions to list of list of sets for speed
    transitions = [[set(e) for e in t] for t in transitions_list]
    tree = RuleTree(n_states*n_states,numNeighbors[neighborhood])
    # for each transition pair, see if we can apply them both at once to a square
    for i,t1 in enumerate(transitions): # as lower
        golly.show("Building rule tree... (pass 1 of 2: "+str(100*i//len(transitions))+"%)")
        for t2 in transitions: # as upper
            # we can only apply both rules at once if they overlap to some extent
            ### AKT: any() and isdisjoint() are not available in Python 2.3:
            ### if any( t1[j].isdisjoint(t2[k]) for j,k in lower2upper[neighborhood] ):
            ###     continue
            any_disjoint = False
            for j,k in lower2upper[neighborhood]:
                if len(t1[j] & t2[k]) == 0:
                    any_disjoint = True
                    break
            if any_disjoint: continue
            # take the intersection of their inputs
            if neighborhood=="triangularVonNeumann":
                tree.add_rule( [ encode(t1[0]&t2[1],t1[1]&t2[0]), # C
                     encode(list(range(n_states)),t1[2]), # S
                     encode(t2[3],list(range(n_states))), # E
                     encode(list(range(n_states)),t1[3]), # W
                     encode(t2[2],list(range(n_states))) ], # N
                     encode(t1[4],t2[4])[0] ) # C'
            elif neighborhood=="triangularMoore":
                tree.add_rule( [ encode(t1[0]&t2[1],t1[1]&t2[0]), # C
                             encode(t1[7],t1[2]&t2[12]), # S
                             encode(t1[4]&t2[3],t2[9]), # E
                             encode(t1[9],t1[3]&t2[4]), # W
                             encode(t1[12]&t2[2],t2[7]), # N
                             encode(t1[6]&t2[11],t1[5]&t2[10]), # SE
                             encode(list(range(n_states)),t1[8]), # SW
                             encode(t2[8],list(range(n_states))), # NE
                             encode(t1[10]&t2[5],t1[11]&t2[6]) ], # NW
                           encode(t1[13],t2[13])[0] ) # C'
    # apply each transition to an individual triangle, leaving the other unchanged
    for i,t in enumerate(transitions):
        golly.show("Building rule tree... (pass 2 of 2: "+str(100*i//len(transitions))+"%)")
        for t_1 in t[1]:
            if neighborhood=="triangularVonNeumann":
                # as lower triangle:
                tree.add_rule( [ encode(t[0],[t_1]), # C
                                 encode(list(range(n_states)),t[2]), # S
                                 list(range(n_states*n_states)), # E
                                 encode(list(range(n_states)),t[3]), # W
                                 list(range(n_states*n_states)) ], # N
                                 encode(t[4],[t_1])[0] ) # C'
                # as upper triangle:
                tree.add_rule( [ encode([t_1],t[0]), # C
                                 list(range(n_states*n_states)), # S
                                 encode(t[3],list(range(n_states))), # E
                                 list(range(n_states*n_states)), # W
                                 encode(t[2],list(range(n_states))) ], # N
                                 encode([t_1],t[4])[0] ) # C'
            elif neighborhood=="triangularMoore":
                # as lower triangle:
                tree.add_rule( [encode(t[0],[t_1]), # C
                    encode(t[7],t[2]), # S
                    encode(t[4],list(range(n_states))), # E
                    encode(t[9],t[3]), # W
                    encode(t[12],list(range(n_states))), # N
                    encode(t[6],t[5]), # SE
                    encode(list(range(n_states)),t[8]), # SW
                    list(range(n_states*n_states)), # NE
                    encode(t[10],t[11]) ], # NW
                    encode(t[13],[t_1])[0] ) # C'
                # as upper triangle:
                tree.add_rule( [encode([t_1],t[0]),
                    encode(list(range(n_states)),t[12]), # S
                    encode(t[3],t[9]), # E
                    encode(list(range(n_states)),t[4]), # W
                    encode(t[2],t[7]), # N
                    encode(t[11],t[10]), # SE
                    list(range(n_states*n_states)), # SW
                    encode(t[8],list(range(n_states))), # NE
                    encode(t[5],t[6]) ], # NW
                    encode([t_1],t[13])[0] ) # C'

    # output the rule tree
    golly.show("Compressing rule tree and saving to file...")
    tree.write(golly.getdir('rules') + rule_name + '.tree')

def TriangularTransitionsToRuleTree_CheckerboardMethod(neighborhood,n_states,transitions,rule_name):

    # Background state 0 has no checkerboard, we infer it from its neighboring cells.
    def encode_lower(s):
        return s
    def encode_upper(s):
        ### AKT: this code causes syntax error in Python 2.3:
        ### return [0 if se==0 else n_states+se-1 for se in s]
        temp = []
        for se in s:
            if se==0:
                temp.append(0)
            else:
                temp.append(n_states+se-1)
        return temp

    total_states = n_states*2 - 1
    if total_states>256:
        golly.warn("Number of states exceeds Golly's limit of 256!")
        golly.exit()

    tree = RuleTree(total_states,4)
    for t in transitions:
        # as lower
        tree.add_rule([encode_lower(t[0]),   # C
                       encode_upper(t[2]),   # S
                       encode_upper(t[1]),   # E
                       encode_upper(t[3]),   # W
                       list(range(total_states))],   # N
                      encode_lower(t[4])[0]) # C'
        # as upper
        tree.add_rule([encode_upper(t[0]),   # C
                       list(range(total_states)),    # S
                       encode_lower(t[3]),   # E
                       encode_lower(t[1]),   # W
                       encode_lower(t[2])],  # N
                      encode_upper(t[4])[0]) # C'

    # output the rule tree
    golly.show("Compressing rule tree and saving to file...")
    tree.write(golly.getdir('rules') + rule_name + '.tree')

def MakeTriangularIcons_SplittingMethod(n_states,colors,force_background,rule_name):
    
    width = 31*(n_states*n_states-1)
    if force_background and n_states>2:
        width+=31
    height = 53
    pixels = [[(0,0,0) for x in range(width)] for y in range(height)]
    
    for row in range(height):
        for column in range(width):
            if force_background and n_states>2 and column>=width-31:
                # add extra 'icon' filled with the intended background color
                pixels[row][column] = background_color
            else:
                # decide if this pixel is a lower or upper triangle
                iState = int(column//31)
                upper = int((iState+1) // n_states)
                lower = (iState+1) - upper*n_states
                is_upper = False
                is_lower = False
                if row<31:
                    # 31x31 icon
                    if (column-iState*31) > row:
                        is_upper = True
                    elif (column-iState*31) < row:
                        is_lower = True
                elif row<46 and (column-iState*31)<15:
                    # 15x15 icon
                    if (column-iState*31) > row-31:
                        is_upper = True
                    elif (column-iState*31) < row-31:
                        is_lower = True
                elif (column-iState*31)<7:
                    # 7x7 icon
                    if (column-iState*31) > row-46:
                        is_upper = True
                    elif (column-iState*31) < row-46:
                        is_lower = True
                if is_upper:
                    pixels[row][column] = colors[upper]
                elif is_lower:
                    pixels[row][column] = colors[lower]
                else:
                    pixels[row][column] = 0,0,0
    
    return pixels

def MakeTriangularIcons_CheckerboardMethod(n_states,colors,force_background,rule_name):
    
    width = 31*(n_states*2-1)
    if force_background and n_states>2:
        width+=31
    height = 53
    pixels = [[(0,0,0) for x in range(width)] for y in range(height)]
    
    lower31x31 =  [[0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0],
                   [0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0],
                   [0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0],
                   [0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0],
                   [0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0],
                   [0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0],
                   [0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0],
                   [0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0],
                   [0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0],
                   [0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0],
                   [0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0],
                   [0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0],
                   [0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0],
                   [0,0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0],
                   [0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0,0,0,0,0,0,0],
                   [0,0,0,0,0,0,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0,0,0,0,0,0],
                   [0,0,0,0,0,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0,0,0,0,0],
                   [0,0,0,0,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0,0,0,0],
                   [0,0,0,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0,0,0],
                   [0,0,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0,0],
                   [0,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0],
                   [0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0],
                   [1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1],
                   [1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1],
                   [0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0],
                   [0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0],
                   [0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0],
                   [0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0],
                   [0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0],
                   [0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0],
                   [0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0]]
    lower15x15 =  [[0,0,0,0,0,0,0,0,0,0,0,0,0,0,0],
                   [0,0,0,0,0,0,0,0,0,0,0,0,0,0,0],
                   [0,0,0,0,0,0,0,0,0,0,0,0,0,0,0],
                   [0,0,0,0,0,0,0,1,0,0,0,0,0,0,0],
                   [0,0,0,0,0,0,1,1,1,0,0,0,0,0,0],
                   [0,0,0,0,0,1,1,1,1,1,0,0,0,0,0],
                   [0,0,0,0,1,1,1,1,1,1,1,0,0,0,0],
                   [0,0,0,1,1,1,1,1,1,1,1,1,0,0,0],
                   [0,0,1,1,1,1,1,1,1,1,1,1,1,0,0],
                   [0,1,1,1,1,1,1,1,1,1,1,1,1,1,0],
                   [1,1,1,1,1,1,1,1,1,1,1,1,1,1,1],
                   [1,1,1,1,1,1,1,1,1,1,1,1,1,1,1],
                   [0,0,0,0,0,0,0,0,0,0,0,0,0,0,0],
                   [0,0,0,0,0,0,0,0,0,0,0,0,0,0,0],
                   [0,0,0,0,0,0,0,0,0,0,0,0,0,0,0]]
    lower7x7 =    [[0,0,0,0,0,0,0],
                   [0,0,0,0,0,0,0],
                   [0,0,0,1,0,0,0],
                   [0,0,1,1,1,0,0],
                   [0,1,1,1,1,1,0],
                   [1,1,1,1,1,1,1],
                   [0,0,0,0,0,0,0]]
    
    if force_background:
        bg_color = colors[0]
    else:
        bg_color = (0,0,0)
    for i in range(1,n_states):
        fg_color = colors[i]
        # draw 31x31 icons
        for row in range(31):
            for column in range(31):
                # draw lower triangle icons
                pixels[row][31*(i-1) + column] = [bg_color,fg_color][lower31x31[row][column]]
                # draw upper triangle icons
                pixels[row][31*(n_states+i-2) + column] = [bg_color,fg_color][lower31x31[28-row][column]]
        # draw 15x15 icons
        for row in range(15):
            for column in range(15):
                # draw lower triangle icons
                pixels[31+row][31*(i-1) + column] = [bg_color,fg_color][lower15x15[row][column]]
                # draw upper triangle icons
                pixels[31+row][31*(n_states+i-2) + column] = [bg_color,fg_color][lower15x15[13-row][column]]
        # draw 7x7 icons
        for row in range(7):
            for column in range(7):
                # draw lower triangle icons
                pixels[46+row][31*(i-1) + column] = [bg_color,fg_color][lower7x7[row][column]]
                # draw upper triangle icons
                pixels[46+row][31*(n_states+i-2) + column] = [bg_color,fg_color][lower7x7[6-row][column]]
    
    return pixels

def EmulateTriangular(neighborhood,n_states,transitions_list,input_filename):
    '''Emulate a triangularVonNeumann or triangularMoore neighborhood rule table with a rule tree.'''

    input_rulename = os.path.splitext(os.path.split(input_filename)[1])[0]

    # read rule_name+'.colors' file if it exists
    force_background = False
    background_color = [0,0,0]
    cfn = os.path.split(input_filename)[0] + "/" + input_rulename + ".colors"
    try:
        cf = open(cfn,'r')
    except IOError:
        # use Golly's default random colours
        random_colors=[[0,0,0],[0,255,127],[127,0,255],[148,148,148],[128,255,0],[255,0,128],[0,128,255],[1,159,0],
            [159,0,1],[255,254,96],[0,1,159],[96,255,254],[254,96,255],[126,125,21],[21,126,125],[125,21,126],
            [255,116,116],[116,255,116],[116,116,255],[228,227,0],[28,255,27],[255,27,28],[0,228,227],
            [227,0,228],[27,28,255],[59,59,59],[234,195,176],[175,196,255],[171,194,68],[194,68,171],
            [68,171,194],[72,184,71],[184,71,72],[71,72,184],[169,255,188],[252,179,63],[63,252,179],
            [179,63,252],[80,9,0],[0,80,9],[9,0,80],[255,175,250],[199,134,213],[115,100,95],[188,163,0],
            [0,188,163],[163,0,188],[203,73,0],[0,203,73],[73,0,203],[94,189,0],[189,0,94]]
        colors = dict(list(zip(list(range(len(random_colors))),random_colors)))
    else:
        # read from the .colors file
        colors = {0:[0,0,0]} # background is black
        for line in cf:
            if line[0:6]=='color ':
                entries = list(map(int,line[6:].replace('=',' ').replace('\n',' ').split()))
                if len(entries)<4:
                    continue # too few entries, ignore
                if entries[0]==0:
                    force_background = True
                    background_color = [entries[1],entries[2],entries[3]]
                else:
                    colors.update({entries[0]:[entries[1],entries[2],entries[3]]})
        # (we don't support gradients in .colors)

    rule_name = input_rulename + '_emulated'
    # (we use a special suffix to avoid picking up any existing .colors or .icons)

    # make a rule tree and some icons
    if n_states <= 16:
        TriangularTransitionsToRuleTree_SplittingMethod(neighborhood,n_states,transitions_list,rule_name)
        pixels = MakeTriangularIcons_SplittingMethod(n_states,colors,force_background,rule_name)
        total_states = n_states * n_states
    elif neighborhood=='triangularVonNeumann' and n_states <= 128:
        TriangularTransitionsToRuleTree_CheckerboardMethod(neighborhood,n_states,transitions_list,rule_name)
        pixels = MakeTriangularIcons_CheckerboardMethod(n_states,colors,force_background,rule_name)
        total_states = n_states * 2 - 1
    else:
        golly.warn('Only support triangularMoore with 16 states or fewer, and triangularVonNeumann\n'+\
                   'with 128 states or fewer.')
        golly.exit()

    if n_states==2:
        # the icons we wrote are monochrome, so we need a .colors file to avoid
        # them all having different colors or similar from Golly's preferences
        c = open(golly.getdir('rules')+rule_name+".colors",'w')
        if force_background:
            c.write('color = 0 '+' '.join(map(str,background_color))+'\n')
        for i in range(1,total_states):
            c.write('color = '+str(i)+' '+' '.join(map(str,colors[1]))+'\n')
        c.flush()
        c.close()
    
    # use rule_name.tree and rule_name.colors and icon info to create rule_name.rule
    ConvertTreeToRule(rule_name, total_states, pixels)
    return rule_name
