import golly
import os
from glife.ReadRuleTable import *
from glife.RuleTree import *
from glife.EmulateTriangularVonNeumann import *
from glife.EmulateMargolus import *

# ask user to select .table file
filename = golly.opendialog('Open a rule table to convert:',
                      'Rule tables (*.table)|*.table',
                      golly.getdir('rules'))

if len(filename) == 0: golly.exit()    # user hit Cancel

# add new converters here as they become available:
# (TODO: triangularMoore, hexagonal, etc.)
Converters = {
    "vonNeumann":ConvertRuleTableTransitionsToRuleTree,
    "Moore":ConvertRuleTableTransitionsToRuleTree,
    "triangularVonNeumann":EmulateTriangularVonNeumann,
    "Margolus":EmulateMargolus,
    "square4_figure8v":EmulateMargolus,
    "square4_figure8h":EmulateMargolus,
    "square4_cyclic":EmulateMargolus,
}

golly.show("Reading from rule table file...")
n_states, neighborhood, transitions = ReadRuleTable(filename)

if not neighborhood in Converters:
    golly.warn("Unsupported neighborhood: "+neighborhood)
    golly.show('')
    golly.exit()
    
rule_name = os.path.splitext(os.path.split(filename)[1])[0] + "_tree"
# (without suffix get confusion between original .colors/.icons and new ones)

golly.show("Building rule tree...")
Converters[neighborhood]( neighborhood, 
                          n_states, 
                          transitions, 
                          filename, 
                          rule_name )
                          
golly.setalgo('RuleTree')
golly.setrule(rule_name)
golly.show('Created '+rule_name+'.tree and selected that rule.')
