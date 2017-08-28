-- Fast LifeHistory to Life converter by Michael Simkin,
--   intended to be mapped to a keyboard shortcut, e.g., Alt+J
-- Sanity checks and Lua translation by Dave Greene, May 2017
-- Creates special rule and runs it for one generation, then switches to Life 
-- Replace 2k + 1-> 1 and 2k -> 0
-- Preserves step and generation count

local g = golly()

local rule = g.getrule()
-- No effect if already in B3/S23 rule
if g.getrule()=="B3/S23" or string.sub(g.getrule(),1,7)=="B3/S23:" then g.exit() end

-- If not in LifeHistory, assume that it's a similar rule with even-numbered OFF states
-- (Edit > Undo will be available to return to the original rule if needed)
if string.sub(g.getrule(),1,11)~="LifeHistory" then g.setrule("LifeHistory") end

ruletext = [[@RULE LifeHistoryToLife
@TABLE
n_states:7
neighborhood:oneDimensional
symmetries:none
var a={0,1,2,3,4,5,6}
var b={a}
var c={2,4,6}
var d={3,5}
c,a,b,0
d,a,b,1]]
   
local function CreateRule()
    local fname = g.getdir("rules").."LifeHistoryToLife.rule"
    local f=io.open(fname,"r")
    if f~=nil then
        io.close(f)  -- rule already exists
    else 
        local f = io.open(fname, "w")
        if f then
            f:write(ruletext)
            f:close()
        else
            g.warn("Can't save LifeHistoryToLife rule in filename:\n"..filename)
        end
    end
end
      
CreateRule()
g.setrule("LifeHistoryToLife")
g.run(1)
step = g.getstep()
g.setrule("Life")
g.setalgo("HashLife")
g.setstep(step)
g.setgen("-1")