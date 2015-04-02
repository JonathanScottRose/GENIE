require "util"

-- Builder

genie.Builder = class()
local g = genie;
local Builder = genie.Builder

function Builder:component(name, modl)
	self.cur_comp = g.Node.new(name, modl)
    self.cur_node = self.cur_comp
end

function Builder:interface(name, type, dir, clock)
	if not self.cur_comp then error("Unexpected 'interface'") end
    
    self.cur_iface = self.cur_comp:add_port(name, type, dir)
end

function Builder:clock_sink(name, binding)
	self:interface(name, 'clock', 'in')
    self:signal('clock', binding, 1)
end

function Builder:reset_sink(name, binding)
	self:interface(name, 'reset', 'in')
    self:signal('reset', binding, 1)
end

function Builder:linkpoint(name, encoding, type)
	if not self.cur_iface then error("Unexpected 'linkpoint'") end
    if not self.cur_iface.add_linkpoint then error("Interface does not support linkpoints") end
    
    self.cur_iface:add_linkpoint(name, encoding, type)
end

function Builder:system(name, topofunc)
	self.cur_sys = g.System.new(name, topofunc)
    self.cur_node = self.cur_sys
end

function Builder:instance(name, comp)
	if not self.cur_sys then error("Unexpected 'instance'") end
    self.cur_node = self.cur_sys:add_node(name, comp)
end

function Builder:export(name, type, dir)
	if not self.cur_sys then error("Unexpected 'export'") end
    self.cur_iface = self.cur_sys:add_port(name, type, dir)
end

function Builder:link(src, dest)
	if not self.cur_sys then error("Unexpected 'link'") end
    self.cur_sys:add_link(src, dest)
end

function Builder:make_exclusive()
end

function Builder:export_if()
end

function Builder:parameter(name, val)
    if not self.cur_node then error("Unexpected 'parameter'") end
    self.cur_node:def_param(name, val)
end

function Builder:signal(role, tag, vname, width)
    if not self.cur_iface then error("Unexpected 'signal'") end
    
    local newargs
    
    if not vname then newargs = {role, tag, 1}
    elseif not width then newargs = {role, tag, vname}
    else newargs = {role, tag, vname, width}
    end
    
    self.cur_iface:add_signal(table.unpack(newargs))
end

