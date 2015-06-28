require "util"

-- 
-- Creates a ring topology. Each existing instance in the given system is given a split and merge node.
-- The split node is an offramp which extracts traffic for the ring destined for the instance's input ports.
-- The merge node takes traffic from the instance's output ports and inects it into the ring.
-- The split node also connects directly to the merge node for traffic that bypasses the instance.
--

function topo_ring(sys)
	--
	-- Create the ring: A split and merge node for every instance
	--
	
	local ring = {}
    
    local objs = sys:get_nodes()
    table.insert(objs, sys)
    
	for obj in values(objs) do
        local objname = obj:get_name()
        
        -- create a ringstop entry for this instance, which contains
        -- a split node and a merge node named after the instance
        local entry = 
        {
            merge = sys:add_merge(objname .. '_inj'),
            split = sys:add_split(objname .. '_ej')
        }
        
        table.insert(ring, entry) -- numerical entry
        ring[obj] = entry -- relational entry
	end
	
    --
    -- Make TOPO connections between the ringstops
    --
    
	for idx,entry in ipairs(ring) do
		-- within a ringstop, the split has a bypass path to the merge
        local e = sys:add_link(entry.split:get_port('out'), entry.merge:get_port('in'))
        entry.s2m = e
        
        -- reference the next ringstop, looping around to make a ring
        -- store the nextentry in each entry, linked-list style, for later
        local nextentry = ring[idx+1] or ring[1]
        entry.next = nextentry -- to help traversal
        
        -- across ringstops, the merge connects to the next's split
        local e = sys:add_link(entry.merge:get_port('out'), nextentry.split:get_port('in'))
        entry.m2s = e
	end
	
	--
	-- Make TOPO connections between instances and ringstops, and associate RS links
	--
	
	for _,link in ipairs(sys:get_links('rs')) do
		-- start at source node's ringstop
        local srcnode = link:get_src():get_rs_port():get_parent()
		local cur = ring[srcnode]
		
		-- get onto ring via merge node
        local e = sys:add_link(link:get_src():get_topo_port(), cur.merge:get_port('in'))
        e:add_parent(link)
		
		-- advance to next ringstop's split node (makes the loop condition work properly)
        e = cur.m2s
		e:add_parent(link)
		cur = cur.next
		
		-- keep traveling in ring until we reach destination node's ringstop
        local sinknode = link:get_sink():get_rs_port():get_parent()
		while (cur ~= ring[sinknode]) do
			e = cur.s2m
			e:add_parent(link)
			e = cur.m2s
			e:add_parent(link)
			cur = cur.next
		end
		
		-- get off of ring
        e = sys:add_link(cur.split:get_port('out'), link:get_sink():get_topo_port())
		e:add_parent(link)
	end
end
