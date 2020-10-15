-- Fast [Rule] or [Rule]Super to [Rule]History converter,
--   intended to be mapped to a keyboard shortcut, e.g., Alt+H
-- Sanity checks and Lua translation by Dave Greene, May 2017
-- Creates special rule and runs it for one generation, then switches to Life 
-- Replace 2k + 1-> 1 and 2k -> 0
-- Preserves step and generation count

local g = golly()

local rule = g.getrule()

-- deal with bounded-universe syntax appropriately
ind = string.find(rule, ":")
suffix = rule:sub(ind) or ""
baserule = rule:sub(1,ind-1) or rule

-- No effect if the current rule ends with "History"
if baserule:sub(-7) == "History" then g.exit("The current rulestring already ends with 'History'.") end

-- If rulestring contains "Super" suffix, remove it and continue
if baserule:sub(-5) == "Super" then baserule = baserule:sub(1,#baserule-5) end

ruletext = [[@RULE SuperToHistory
@TABLE
n_states:26
neighborhood:oneDimensional
symmetries:none
var a={0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25}
var b={a}
var withtrailON ={9,11}
var withtrailOFF={10,12}
var notrailON   ={13,15,17,19,21,23,25}
var notrailOFF  ={14,16,18,20,22,24}
var markedON    ={7}
var markedOFF   ={8}

notrailOFF  ,a,b,0
withtrailON ,a,b,1
notrailON   ,a,b,1
withtrailOFF,a,b,2
markedON    ,a,b,3
markedOFF   ,a,b,4]]
   
local function CreateRule()
    local fname = g.getdir("rules").."SuperToHistory.rule"
    local f=io.open(fname,"r")
    if f~=nil then
        io.close(f)  -- rule already exists
    else 
        local f = io.open(fname, "w")
        if f then
            f:write(ruletext)
            f:close()
        else
            g.warn("Can't save SuperToHistory rule in filename:\n"..filename)
        end
    end
end
      
CreateRule()
g.setrule("SuperToHistory")
g.run(1)
step = g.getstep()

-- if you get the error "Given rule is not valid in any algorithm" on this next line
--   you might be attempting to run this script in a version of Golly before 4.0.
g.setrule(baserule.."History"..suffix)

g.setalgo("Super")
g.setstep(step)
g.setgen("-1")
