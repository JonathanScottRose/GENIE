require 'util'

-- 
-- Builds a simple crossbar-like topology with up to 1 layer of split nodes
-- and up to 1 layer of merge nodes
--

function topo_xbar(sys)
    -- maps each RS link to the furthest-along (forward or backward) TOPO port
    -- that's carrying that RS link
	local heads = {}
	local tails = {}
	
    -- given a SET of RS links, return a table with:
    -- key: head/tail TOPO port
    -- value: set of RS links associated with said TOPO port
    local function gather_X(X)
        return function(links)
            local result = {}
            for link in Set.values(links) do
                local port = X[link]
                result[port] = result[port] or {}
                Set.add(result[port], link)
            end
            return result
        end
    end
    
    local gather_heads = gather_X(heads)
    local gather_tails = gather_X(tails)

	-- main code
	
    --
    -- 1: get all RS links from the system into a set
    --
    
    local rs_links = Set.make(sys:get_links('rs'))
    
    --
	-- 2: initialize the head/tail for each link to be at its ultimate 
	-- TOPO source/destination respectively
    --
    
	for link in Set.values(rs_links) do
		heads[link] = link:get_src():get_topo_port()
		tails[link] = link:get_sink():get_topo_port()
	end

	--
	-- 3: Split nodes
	--

	for src,links in pairs(gather_heads(rs_links)) do
		local sinks = gather_tails(links)
        
        -- this src port connects to multiple sink ports? create split node.
		if util.count(sinks) > 1 then
            local split = sys:add_split(
                util.unique_key(sys:get_objects(), 'split')
            )
          
            -- link src port to split node with a topo link
            local tlink = sys:add_link(src, split:get_port('in'))
			
            -- make the new TOPO link be a child of the RS link
			for link in Set.values(links) do
                link:add_child(tlink)
				heads[link] = split:get_port('out')
			end
		end
	end

	--
	-- 5: Merge nodes
	-- 
    
	for sink,links in pairs(gather_tails(rs_links)) do
		local srcs = gather_heads(links)
		if util.count(srcs) > 1 then
            local merge = sys:add_merge(
                util.unique_key(sys:get_objects(), 'merge')
            )
			
            local tlink = sys:add_link(merge:get_port('out'), sink)
			
			for link in Set.values(links) do
                link:add_child(tlink)
				tails[link] = merge:get_port('in')
			end
		end
	end

	--
	-- 6: Connect each link's head to its tail
	--

	for head,links in pairs(gather_heads(rs_links)) do
        -- (all links from one head) -> sort by tail
		local tails = gather_tails(links)
        for tail,links2 in pairs(tails) do
            local tlink = sys:add_link(head, tail)
        
            -- carry all the associated RS links over this new TOPO link
            for link in Set.values(links2) do
                link:add_child(tlink)
            end
        end
	end
end
