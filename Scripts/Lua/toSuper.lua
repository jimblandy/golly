-- [Rule] or [Rule]History to [Rule]Super converter,
--   intended to be mapped to a keyboard shortcut, e.g., Alt+G

local g = golly()

local rule = g.getrule()

-- deal with bounded-universe syntax appropriately
suffix = ""
baserule = rule
ind = string.find(rule, ":")
if (ind) then
    suffix = rule:sub(ind)
    baserule = rule:sub(1,ind-1)
end

-- No effect if the current rule ends with "Super"
if baserule:sub(-5) == "Super" then g.exit("The current rulestring already ends with 'Super'.") end

-- If rulestring contains "History" suffix, remove it and continue
if baserule:sub(-7) == "History" then baserule = baserule:sub(1, #baserule-7) end

g.setrule(baserule.."Super"..suffix)
g.setalgo("Super")