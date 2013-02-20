import golly
import os
from glife.ReadRuleTable import *
from glife.RuleTree import *
from glife.EmulateTriangular import *
from glife.EmulateMargolus import *
from glife.EmulateOneDimensional import *
from glife.EmulateHexagonal import *

# ask user to select .table file
filename = golly.opendialog('Open a rule table to convert:',
                      'Rule tables (*.table)|*.table',
                      golly.getdir('rules'))

if len(filename) == 0: golly.exit()    # user hit Cancel

# add new converters here as they become available:
Converters = {
    "vonNeumann":ConvertRuleTableTransitionsToRuleTree,
    "Moore":ConvertRuleTableTransitionsToRuleTree,
    "triangularVonNeumann":EmulateTriangular,
    "triangularMoore":EmulateTriangular,
    "Margolus":EmulateMargolus,
    "square4_figure8v":EmulateMargolus,
    "square4_figure8h":EmulateMargolus,
    "square4_cyclic":EmulateMargolus,
    "oneDimensional":EmulateOneDimensional,
    "hexagonal":EmulateHexagonal,
}

golly.show("Reading from rule table file...")
n_states, neighborhood, transitions = ReadRuleTable(filename)

if not neighborhood in Converters:
    golly.warn("Unsupported neighborhood: "+neighborhood)
    golly.show('')
    golly.exit()

# all converters now create a .rule file
golly.show("Building rule...")
rule_name = Converters[neighborhood]( neighborhood,
                                      n_states,
                                      transitions,
                                      filename )

golly.new(rule_name+'-demo.rle')
golly.setalgo('RuleLoader')
golly.setrule(rule_name)
golly.show('Created '+rule_name+'.rule and selected that rule.')
