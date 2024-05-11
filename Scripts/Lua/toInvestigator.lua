-- Fast [Rule] to [Rule]Investigator converter,
-- intended to be mapped to a keyboard shortcut, e.g., Alt+I
-- v1.0, 11 May 2024: initial version

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

-- No effect if the current rule ends with "Investigator"
if algo == "Super" then
    if baserule:sub(-12) == "Investigator" then
        g.exit("The current rule is already a [Rule]Investigator rule.")
    elseif baserule:sub(-7) == "History" or baserule:sub(-5) == "Super" then
        g.exit("Please use Alt/Option+J (toStandard.lua) to convert to a Standard rule "..
            "before using this script to convert the pattern to an Investigator rule.")
    end
end

step = g.getstep()

-- attempt to set the new Investigator rule to see if it is valid
local function tryrule()
    g.setrule(baserule.."Investigator"..suffix)
end
local status, err = pcall(tryrule)
if err then
    g.note("Conversion failed. This '"..baserule.."' rule is not supported by the Super algo.\n"
           .. "To revert to the original rule, please click OK, then press Z to undo the rule change.")
    g.exit("Press Z to revert to the original rule.")
end

g.setstep(step)
