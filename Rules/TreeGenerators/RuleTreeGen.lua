-- This Lua script can be used to create tree data for inclusion in a Golly .rule file.
-- (Run it from a command line, not from within Golly.)

--------------------------------------------------------------------------------

function GenerateRuleTree(numStates, numNeighbors, transitionFunc)
    local numParams = numNeighbors + 1
    local world = {}
    local r = {}
    local nodeSeq = 0
    local params = {}
    for i = 0, numParams-1 do params[i] = 0 end

    local function getNode(n)
        if world[n] then return world[n] end
        local new_node = nodeSeq
        nodeSeq = nodeSeq + 1
        r[#r+1] = n
        world[n] = new_node
        return new_node
    end
    
    local function recur(at)
        if at == 0 then return transitionFunc(params) end
        local n = tostring(at)
        for i = 0, numStates-1 do
            params[numParams-at] = i
            n = n.." "..recur(at-1)
        end
        return getNode(n)
    end
    
    recur(numParams)
    print("num_states="..numStates)
    print("num_neighbors="..numNeighbors)
    print("num_nodes="..#r)
    for i = 1, #r do print(r[i]) end
end

--------------------------------------------------------------------------------

-- define your own transition function here:

function my_transition_function(a)
    -- this code is for B3/S23
    local n = a[0] + a[1] + a[2] + a[3] + a[4] + a[5] + a[6] + a[7]
    if n == 2 and a[8] ~= 0 then
        return 1
    end
    if n == 3 then
        return 1
    end
    return 0
end

--------------------------------------------------------------------------------

-- call the rule tree generator with your chosen parameters:

local n_states = 2
local n_neighbors = 8
GenerateRuleTree(n_states, n_neighbors, my_transition_function)
