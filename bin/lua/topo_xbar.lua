--- Sparse Crossbar topology.
-- Attaches a Split node to every output that has more than one physical fanout.
-- Attaches a Merge node to every input that has more than one physical fanin.
-- Connects the Splits to the Merges, with optional Buffer Nodes after each Split or Merge.
--@usage
--b:system('mysys', topo_xbar)
--@usage
--b:system('mysys', make_topo_xbar(true, false, true))
-- @module topo_xbar


require 'util'

-- 
-- Builds a simple crossbar-like topology with up to 1 layer of split nodes
-- and up to 1 layer of merge nodes
--

--- Default version that adds buffers after merge nodes only.
-- @tparam System sys
function topo_xbar(sys)
    make_topo_xbar(false, true, false)(sys)
end

--- Returns a customized topo_xbar topology function.
-- You get to control where buffer nodes are placed.
-- @tparam boolean reg_split Add buffer at input to split nodes.
-- @tparam boolean reg_merge Add buffer at output of merge nodes.
-- @tparam boolean reg_internal Add buffer between each split and each merge.
-- @treturn function
function make_topo_xbar(reg_split, reg_merge, reg_internal)
return function(sys)
    
    -- maps each RS link to the furthest-along (forward or backward) TOPO port
    -- that's carrying that RS link
	local heads = {}
	local tails = {}
	local sm_count = {}
    
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
    -- 1: get all RS links from the system (that haven't been topologized yet) into a set
    --
    
    local rs_links = Set.make(sys:get_untopo_rs_links())
	
    --
	-- 2: initialize the head/tail for each link to be at its ultimate 
	-- TOPO source/destination respectively
    --
    
	for link in Set.values(rs_links) do
		heads[link] = link:get_src():get_topo_port()
		tails[link] = link:get_sink():get_topo_port()
        sm_count[link] = 0
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
            local tlink = sys:add_link(src, split)
			
            -- make the new TOPO link be a child of the RS link
			for link in Set.values(links) do
				heads[link] = split
                sm_count[link] = sm_count[link] + 1
			end
            
            if reg_split then
                local reg = sys:add_buffer(split:get_name() .. "_reg")
                sys:splice_node(tlink, reg)
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
			
            local tlink = sys:add_link(merge, sink)

			for link in Set.values(links) do
				tails[link] = merge
                sm_count[link] = sm_count[link] + 1
			end
            
            if reg_merge then
                local reg = sys:add_buffer(merge:get_name() .. "_reg")
                sys:splice_node(tlink, reg)
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
       
            if reg_internal and sm_count[next(links2)] == 2 then
                local reg = sys:add_buffer(
                    util.unique_key(sys:get_objects(), 'intreg')
                )
                sys:splice_node(tlink, reg)
            end
        end
	end
	
end
end
