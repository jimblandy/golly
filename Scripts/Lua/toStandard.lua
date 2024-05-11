-- Fast [Rule][History|Super] to [Rule] converter by Michael Simkin,
--   intended to be mapped to a keyboard shortcut, e.g., Alt+J
-- Sanity checks and Lua translation by Dave Greene, May 2017
-- Creates special rule and runs it for one generation, then switches to Life 
-- Replace 2k + 1-> 1 and 2k -> 0
-- Preserves step and generation count
-- v1.1, 11 May 2024: support [Rule]Investigator patterns

local g = golly()

local rule = g.getrule()
local algo = g.getalgo()

local function CreateRule(filename, ruletext)
    local fname = g.getdir("rules")..filename..".rule"
    local f=io.open(fname,"r")
    if f~=nil then
        io.close(f)  -- rule already exists
    else 
        local f = io.open(fname, "w")
        if f then
            f:write(ruletext)
            f:close()
        else
            g.warn("Can't save SuperToStandard rule in filename:\n"..filename)
        end
    end
end

-- deal with bounded-universe syntax appropriately
ind = string.find(rule, ":")
suffix = ""
baserule = rule
if ind then
    suffix = rule:sub(ind)
    baserule = rule:sub(1,ind-1)
end

-- No effect if the current rule is not a Super algo rule
if algo ~= "Super" then g.exit("The current rule is not a [Rule]History, [Rule]Super, or [Rule]Investigator rule.") end

ruletextSuper = [[@RULE SuperToStandard
@TABLE
n_states:26
neighborhood:oneDimensional
symmetries:none
var a={0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25}
var b={a}
var c={2,4,6,8,10,12,14,16,18,20,22,24}
var d={3,5,7,9,11,13,15,17,19,21,23,25}
c,a,b,0
d,a,b,1]]

CreateRule("SuperToStandard", ruletextSuper)

ruletextInvestigator = [[@RULE InvestigatorToStandard
@TABLE
n_states:21
neighborhood:oneDimensional
symmetries:none
var a={0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20}
var b={a}
var c={2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20}
c,a,b,0]]

CreateRule("InvestigatorToStandard", ruletextInvestigator)

-- If rulestring contains "Super", "History", or "Investigator" suffix, remove it and continue
if baserule:sub(-5) == "Super" then
    baserule = baserule:sub(1,#baserule-5)
    g.setrule("SuperToStandard")
elseif baserule:sub(-7) == "History" then
    baserule = baserule:sub(1, #baserule-7)
    g.setrule("SuperToStandard")
elseif baserule:sub(-12) == "Investigator" then
    baserule = baserule:sub(1,#baserule-12)
    g.setrule("InvestigatorToStandard")
else
    g.show("Rule is not one of the known formats supported by the Super algorithm.")
    g.exit()
end

-- If rulestring is "LifeV" then convert it to B3/S23V
if baserule:lower() == "lifev" then baserule = "B3/S23V" end

-- If rulestring is "LifeH" then convert it to B3/S23H
if baserule:lower() == "lifeh" then baserule = "B3/S23H" end

g.run(1)
step = g.getstep()

g.setrule(baserule .. suffix)
g.setalgo("HashLife")
g.setstep(step)
g.setgen("-1")