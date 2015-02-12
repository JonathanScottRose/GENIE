require "util"
require "spec"

-- Create this package and set up environment

if not topo then topo = {} end
setmetatable(topo, {__index = _ENV})
_ENV = topo


-- Node

Node = class
{
	Types = Set.make {'EP', 'SPLIT', 'MERGE'},
	
	name = nil,
	type = nil,
	ins = {},
	outs = {}
}

function Node:new(o)
	o = Node:_init_inst(o)
	o.ins = {}
	o.outs = {}
	return o
end

function Node:add_in(x)
	table.insert(self.ins, x)
end

function Node:add_out(x)
	table.insert(self.outs, x)
end

-- Edge

Edge = class
{
	src = nil,
	dest = nil,
	links = {}
}

function Edge:new(o)
	o = Edge:_init_inst(o)
	o.links = {}
	return o
end

function Edge:add_link(link)
	table.insert(self.links, link)
end

-- Graph

Graph = class
{
	nodes = {},
	edges = {}
}

function Graph:new(o)
	o = Graph:_init_inst(o)
	o.nodes = {}
	o.edges = {}
	return o
end

function Graph:init(sys)
	self.sys = sys
	for link in values(sys.links) do
		for _,x in ipairs({link.src, link.dest}) do			
			local name = x:phys()
			
			self.nodes[name] = self.nodes[name] or Node:new
			{
				name = name,
				type = 'EP'
			}
		end
	end
end

function Graph:add_node(n)
	util.insert_unique(n, self.nodes)
	return n -- heck, why not
end

function Graph:connect(s,d)
	s = self.nodes[s] or s -- allows s and d to be specified by name or by entry interchangeably
	d = self.nodes[d] or d
	
	-- return existing edge if there is one
	local edge = self:get_edge(s,d)
	if edge then return edge end
	
	edge = Edge:new { src = s.name, dest = d.name }
	s:add_out(edge)
	d:add_in(edge)
	table.insert(self.edges, edge)
	return edge
end

function Graph:get_edge(s,d)
	s = self.nodes[s] or s
	d = self.nodes[d] or d
	for edge in values(s.outs) do
		if edge.dest == d.name then return edge end
	end
	return nil
end

function Graph:dump_dot(filename)
	io.output(filename)
	io.write('digraph {\n')
	for edge in values(self.edges) do
		io.write(string.format('"%s" -> "%s" [label="%s"];\n', edge.src, edge.dest, util.count(edge.links)))
	end
	io.write('}')
	io.close()
end

function Graph:submit()
	for nodename,node in spairsk(self.nodes) do
		genie.create_topo_node(self.sys.name, nodename, node.type)
	end
	
	for edge in svaluesk(self.edges) do
		local linkarray = {}
		for link in svaluesk(edge.links) do
			table.insert(linkarray, {src = link.src:str(), dest = link.dest:str()})
		end
		genie.create_topo_edge(self.sys.name, edge.src, edge.dest, linkarray)
	end
end




