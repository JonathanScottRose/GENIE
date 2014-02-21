require "spec"
require "util"
require "topo"

-- 
-- Builds a simple crossbar-like topology with up to 1 layer of split nodes
-- and up to 1 layer of merge nodes
--

function topo_xbar(sys)
	-- graph to be constructed and returned
	local graph = topo.Graph:new()
	
	-- for each virtual link in the system spec, these keep track
	-- of the nodes in the graph that are the furthest-along feeders
	-- or sinks of those links
	local heads = {}
	local tails = {}

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
	
	-- helper functions: gather all the sources/sinks of a set of links into one set
	local function gather_srcs(links)
		local result = {}
		for link in keys(links) do
			Set.add(result, link.src)
		end
		return result
	end

	function gather_dests(links)
		local result = {}
		for link in keys(links) do
			Set.add(result, link.dest)
		end
		return result
	end

	-- main code
	
	-- 1: initialize graph nodes from system spec, creating nodes for each
	-- instance's interface
	graph:init(sys)
	
	-- 2: initialize the head/tail for each link to be at its ultimate 
	-- source/destination respectively
	for link in keys(sys.links) do
		heads[link] = link.src:phys()
		tails[link] = link.dest:phys()
	end


	--
	-- 3: Direct connection for clocks and resets
	--
	for link,targ in pairs(heads) do
		local t = get_iface_type(sys, targ)
		if t == 'CLOCK' or t == 'RESET' or t == 'CONDUIT' then
			-- make direct connection
			local e = graph:connect(link.src:phys(), link.dest:phys())
			Set.add(e.links, link)
			
			-- delete this link 
			heads[link] = nil
			tails[link] = nil
		end
	end

	--
	-- 4: Split nodes
	--
	local rheads = {}
	for link,targ in pairs(heads) do
		rheads[targ] = rheads[targ] or {}
		Set.add(rheads[targ], link)
	end

	for src,links in pairs(rheads) do
		local dests = gather_dests(links)
		if util.count(dests) > 1 then
			local split = topo.Node:new
			{
				name = util.unique_key(graph.nodes, 'split'),
				type = 'SPLIT'
			}
			
			graph.nodes[split.name] = split
			local edge = graph:connect(src, split.name)
			
			for link in keys(links) do
				Set.add(edge.links, link)
				heads[link] = split.name
			end
		end
	end

	--
	-- 5: Merge nodes
	-- 

	local rtails = {}
	for link,targ in pairs(tails) do
		rtails[targ] = rtails[targ] or {}
		Set.add(rtails[targ], link)
	end

	for dest,links in pairs(rtails) do
		local srcs = gather_srcs(links)
		if util.count(srcs) > 1 then
			local merge = topo.Node:new
			{
				name = util.unique_key(graph.nodes, 'merge'),
				type = 'MERGE'
			}
			
			graph.nodes[merge.name] = merge
			local edge = graph:connect(merge.name, dest)
			
			for link in keys(links) do
				Set.add(edge.links, link)
				tails[link] = merge.name
			end
		end
	end

	--
	-- 6: Connect current heads to tails
	--

	for link,src in pairs(heads) do
		local dest = tails[link]
		
		local edge = graph:get_edge(src, dest) or graph:connect(src, dest)
		Set.add(edge.links, link)
	end
	
	return graph
end