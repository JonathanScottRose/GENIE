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
	Set.add(self.links, link)
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
	for link in keys(sys.links) do
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
	s = self.nodes[s] or s
	d = self.nodes[d] or d
	local newedge = self:get_edge(s,d) or Edge:new { src = s.name, dest = d.name }
	Set.add(s.outs, newedge)
	Set.add(d.ins, newedge)
	Set.add(self.edges, newedge)
	return newedge
end

function Graph:get_edge(s,d)
	s = self.nodes[s] or s
	d = self.nodes[d] or d
	for edge in keys(s.outs) do
		if edge.dest == d.name then return edge end
	end
	return nil
end

function Graph:dump_dot(filename)
	io.output(filename)
	io.write('digraph {\n')
	for edge in keys(self.edges) do
		io.write(string.format('"%s" -> "%s" [label="%s"];\n', edge.src, edge.dest, util.count(edge.links)))
	end
	io.write('}')
	io.close()
end

function Graph:submit()
	for nodename,node in pairs(self.nodes) do
		ct.create_topo_node(self.sys.name, nodename, node.type)
	end
	
	for edge in Set.values(self.edges) do
		local linkarray = {}
		for link in Set.values(edge.links) do
			table.insert(linkarray, {src = link.src:str(), dest = link.dest:str()})
		end
		ct.create_topo_edge(self.sys.name, edge.src, edge.dest, linkarray)
	end
end




