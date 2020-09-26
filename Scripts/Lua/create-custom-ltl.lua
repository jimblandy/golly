-- Use the current selection to create a Larger than Life rule
-- with a Custom neighborhood
--
-- Instructions:
-- 1. Draw the neigborhood as cells
-- 2. Make a square selection containing the cells
-- 3. Run this script and a rule is created with the correct
--    range and custom neighborhood definition
--
-- Selection must be square and have a side length which is an
-- odd number from 3 to 81 (which equates to ranges 1 to 40)
--
-- Author: Chris Rowett (crowett@gmail.com), Sep 2020

local g = golly()
local minlen = 3
local maxlen = 81

-- check for selection
local selrect = g.getselrect()
if #selrect == 0 then
    g.warn("Please make a square selection and try again.", false)
    g.exit("")
end

-- selection must be square, with an odd side length from 3 to 99
local x, y, wd, ht = table.unpack(selrect)
if wd ~= ht then
    g.warn("Selection must be square.", false)
    g.exit("")
end
if (wd & 1) ~= 1 then
    g.warn("Selection dimension must be odd.", false)
    g.exit("")
end
if wd < minlen or wd > maxlen then
    g.warn("Selection dimension must be from "..minlen.." to "..maxlen..".", false)
    g.exit("")
end

-- hex characters
local hexcharacters = "0123456789abcdef"

-- get the list of alive cells in the selection
local selcells = g.getcells(selrect)

-- build a bitmap of set cells
local bitmap = {}
for i = 1, wd * ht do
    bitmap[i] = 0
end

-- populate bitmap from cells
local inc = 2
if (#selcells & 1) == 1 then inc = 3 end

for i = 1, #selcells - 1, inc do
    offset = (selcells[i] - x) + wd * (selcells[i + 1] - y) + 1
    bitmap[offset] = 1
end

-- build the hexadecimal custom neighborhood string from the bitmap
local midy = ht // 2 + 1
local midx = wd // 2 + 1
local i = 3;
local index = 1
local value = 0
local output = ""
for y = 1, ht do
    for x = 1, wd do
        -- ignore the middle cell
        if not (y == midy and x == midx) then
            -- convert each 4 cells into a hex digit
            state = bitmap[index]
            if state > 0 then
                value = value | (1 << i)
            end
            i = i - 1
            if i < 0 then
                output = output..hexcharacters:sub(value + 1, value + 1)
                i = 3
                value = 0
            end
        end
        index = index + 1
    end
end

-- create a Custom rule with the neighborhood and range
local rule = g.getrule()
local algo = g.getalgo()
local numstates = g.numstates()

-- check whether to create an LtL or HROT format rule
if algo == "Larger than Life" and rule:find(",M") then
    rule = "R"..(wd // 2)..",C"..numstates..",M0,S1..1,B1..1,N@"..output
    g.setrule(rule)
else
    rule = "R"..(wd // 2)..",C"..numstates..",S1,B1,N@"..output
    g.setrule(rule)
end
g.note("Created Larger than Life rule with Custom Neighborhood:\n\n"..rule, false)
