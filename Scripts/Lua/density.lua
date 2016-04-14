-- Calculates the density of live cells in the current pattern.
-- Author: Andrew Trevorrow (andrew@trevorrow.com), March 2016.

local g = golly()

local bbox = g.getrect()
if #bbox == 0 then g.exit("The pattern is empty.") end

local d = g.getpop() / (bbox[3] * bbox[4])
if d < 0.000001 then
    g.show(string.format("Density = %.1e", d))
else
    g.show(string.format("Density = %.6f", d))
end
