-- [Rule] or [Rule]History to [Rule]Super converter,
--   intended to be mapped to a keyboard shortcut, e.g., Alt+G
-- v1.1: better error-checking by GUYTU6J, 30 April 2023
-- v1.2, 11 May 2024: support [Rule]Investigator patterns

local g = golly()

local rule = g.getrule()
local algo = g.getalgo()

-- deal with bounded-universe syntax appropriately
suffix = ""
baserule = rule
ind = string.find(rule, ":")
if ind then
    suffix = rule:sub(ind)
    baserule = rule:sub(1,ind-1)
end

-- No effect if the current rule ends with "Super"
if algo == "Super" and baserule:sub(-5) == "Super" then
    g.exit("The current rule is already a [Rule]Super rule.")
end

step = g.getstep()
-- If rulestring contains "History" suffix, remove it and continue
if algo == "Super" then
    if baserule:sub(-7) == "History" then
        baserule = baserule:sub(1, #baserule-7)
    elseif baserule:sub(-12) == "Investigator" then
        g.exit("Please use Alt/Option+J (toStandard.lua) to convert to a Standard rule "..
            "before using this script to convert the pattern to a Super rule.")
    end
end

-- attempt to set the new Super rule to see if it is valid
local function tryrule()
    g.setrule(baserule.."Super"..suffix)
end
local status, err = pcall(tryrule)
if err then
    g.note("Conversion failed. This '"..baserule.."' rule is not supported by the Super algo.\n"
           .. "To revert to the original rule, please click OK, then press Z to undo the rule change.")
    g.exit("Press Z to revert to the original rule.")
end

g.setstep(step)