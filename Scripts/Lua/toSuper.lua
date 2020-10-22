-- [Rule] or [Rule]History to [Rule]Super converter,
--   intended to be mapped to a keyboard shortcut, e.g., Alt+G

local g = golly()

local rule = g.getrule()
local algo = g.getalgo()

-- deal with bounded-universe syntax appropriately
suffix = ""
baserule = rule
ind = string.find(rule, ":")
if (ind) then
    suffix = rule:sub(ind)
    baserule = rule:sub(1,ind-1)
end

-- No effect if the current rule ends with "Super"
if algo == "Super" and baserule:sub(-5) == "Super" then g.exit("The current rule is already a [Rule]Super rule.") end

-- If rulestring contains "History" suffix, remove it and continue
if algo == "Super" and baserule:sub(-7) == "History" then baserule = baserule:sub(1, #baserule-7) end

local function setnewrule()
    g.setrule(baserule.."Super"..suffix)
end
local status, err = pcall(setnewrule)
if err then
    g.exit("The current rule is not supported by the Super algo.")
end