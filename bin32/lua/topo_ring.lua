require "spec"
require "util"
require "topo"

-- 
-- Creates a ring topology. Each INSTANCE in the given system is given a split and merge node.
-- The split node is an offramp which extracts traffic for the ring destined for the instance's input ports.
-- The merge node takes traffic from the instance's output ports and inects it into the ring.
-- The split node also connects directly to the merge node for traffic that bypasses the instance.
--

function topo_ring(sys)
	-- graph to be constructed and returned
	local graph = topo.Graph:new()
	
	-- helper function: get interface type of instance.iface, also the type of object
	local function get_iface_type(sys, targname)
		local targ = spec.LinkTarget:new()
		targ:parse(targname)
	
		local inst = sys.objects[targ.obj]
		if inst.type == 'INSTANCE' then
			local comp = sys.parent.components[inst.component]
			local iface = comp.interfaces[targ.iface]
			return iface.type, inst.type
		else
			return inst.iface_type, inst.type
		end
	end
	
	-- main code
	
	-- initialize graph nodes from system spec, creating nodes for each
	-- instance's interface
	graph:init(sys)
	
	--
	-- Make direct connections for clock, reset, conduit, and export
	--
	local data_links = {}
	
	for link in Set.values(sys.links) do
		local st,sot = get_iface_type(sys, link.src:phys())
		local dt,dot = get_iface_type(sys, link.dest:phys())
		if st == 'CLOCK' or st == 'RESET' or st == 'CONDUIT' or sot == 'EXPORT' or dot == 'EXPORT' then
			-- make direct connection
			local e = graph:connect(link.src:phys(), link.dest:phys())
			Set.add(e.links, link)
		else
			-- data_links is what's left to process
			Set.add(data_links, link)
		end
	end

	--
	-- Create the ring: A split and merge node for every instance
	--
	
	local ring = {}
	
	for objname,obj in pairs(sys.objects) do
		if obj.type == "INSTANCE" then
			-- create split and merge nodes
			local entry = 
			{
				merge = topo.Node:new
				{
					name = objname .. '_inj',
					type = 'MERGE'
				},
				
				split = topo.Node:new
				{
					name = objname .. '_ej',
					type = 'SPLIT'
				}
			}
			
			-- add nodes to graph, connect them with one edge
			graph:add_node(entry.split)
			graph:add_node(entry.merge)
			graph:connect(entry.split, entry.merge)
			
			table.insert(ring, entry) -- numerical entry
			ring[objname] = entry -- relational entry
		end
	end
	
	for idx,entry in ipairs(ring) do
		local nextentry = ring[idx+1] or ring[1] -- loop around
		graph:connect(entry.merge, nextentry.split)
		entry.next = nextentry -- to help traversal
	end
	
	--
	-- Follow end-to-end links through the ring, updating edges to reflect carriage
	--
	
	for link in Set.values(data_links) do
		-- start at source node's ringstop
		local cur = ring[link.src.obj]
		
		-- get onto ring via merge node
		local e = graph:connect(link.src:phys(), cur.merge)
		e:add_link(link)
		
		-- advance to next ringstop's split node (makes the loop condition work properly)
		e = graph:get_edge(cur.merge, cur.next.split)
		e:add_link(link)
		cur = cur.next
		
		-- keep traveling in ring until we reach destination node's ringstop
		while (cur ~= ring[link.dest.obj]) do
			e = graph:get_edge(cur.split, cur.merge)
			e:add_link(link)
			e = graph:get_edge(cur.merge, cur.next.split)
			e:add_link(link)
			cur = cur.next
		end
		
		-- get off of ring
		e = graph:connect(cur.split, link.dest:phys())
		e:add_link(link)
	end
	
	return graph
end