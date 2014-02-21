require "spec"
require "util"
require "topo"

-- 
-- Builds a topology where everthing travels over one shared channel
--

function topo_shared_toob(sys)
	-- graph to be constructed and returned
	local graph = topo.Graph:new()
	
	-- helper function: get interface type of instance.iface
	local function get_iface_type(sys, targname)
		local targ = spec.LinkTarget:new()
		targ:parse(targname)
	
		local inst = sys.objects[targ.obj]
		if inst.type == 'INSTANCE' then
			local comp = sys.parent.components[inst.component]
			local iface = comp.interfaces[targ.iface]
			return iface.type
		else
			return inst.iface_type
		end
	end

	-- main code
	
	-- 1: initialize graph nodes from system spec, creating nodes for each
	-- instance's interface
	graph:init(sys)
	
	-- 2: create a single split and a single merge node, and a physical edge between them
	graph.nodes['split'] = topo.Node:new
	{
		name = 'split',
		type = 'SPLIT'
	}
	
	graph.nodes['merge'] = topo.Node:new
	{
		name = 'merge',
		type = 'MERGE'
	}
	
	local merge_to_split_edge = graph:connect('merge', 'split')
	
	-- 3: go through all links in the spec
	-- make direct connections for clock/reset/conduit links
	-- for data links, make them pass through the merge node and then the split node	
	for link in Set.values(sys.links) do
		local t = get_iface_type(sys, link.src:phys())
		if t == 'CLOCK' or t == 'RESET' or t == 'CONDUIT' then
			-- make direct connection
			local e = graph:connect(link.src:phys(), link.dest:phys())
			e:add_link(link)
		else
			-- connect source to merge node with a new edge, make the edge carry the link
			local e = graph:connect(link.src:phys(), 'merge')
			e:add_link(link)
			
			-- make the single merge-to-split edge carry this link as well
			merge_to_split_edge:add_link(link)
			
			-- connect split node to the final destination
			e = graph:connect('split', link.dest:phys())
			e:add_link(link)
		end
	end
	
	return graph
end