require "util"

-- Builder

genie.Builder = class()
local g = genie;
local Builder = genie.Builder

function Builder:component(name, modl)
    self.cur_node = g.Node.new(name, modl)
    return self.cur_node
end

function Builder:interface(name, type, dir, clock)
	if not self.cur_node then error("Unexpected 'interface'") end
    self.cur_port = self.cur_node:add_port(name, type, dir)
    return self.cur_port
end

-- programmatically define several special-case versions of ::interface,
-- for each network type and direction
for _,i in pairs({{'src', 'out'}, {'sink', 'in'}}) do
    local suffix = i[1]
    local ifdir = i[2]
    
    Builder['clock_'..suffix] = function(self, name, binding)
        local result = self:interface(name, 'clock', ifdir)
        self:signal('clock', binding, 1)
        return result
    end
    
    Builder['reset_'..suffix] = function(self, name, binding)
        local result = self:interface(name, 'reset', ifdir)
        self:signal('reset', binding, 1)
        return result
    end
    
    Builder['conduit_'..suffix] = function(self, name)
        return self:interface(name, 'conduit', ifdir)
    end
    
    Builder['rs_'..suffix] = function(self, name, clockif)
        local result = self:interface(name, 'rs', ifdir)
        result:set_clock_port_name(clockif)
        return result
    end
end

function Builder:linkpoint(name, encoding, type)
	if not self.cur_port then error("Unexpected 'linkpoint'") end
    if not self.cur_port.add_linkpoint then error("Interface does not support linkpoints") end
    
    self.cur_port:add_linkpoint(name, encoding, type)
end

function Builder:system(name, topofunc)
	self.cur_sys = g.System.new(name, topofunc)
    self.cur_node = self.cur_sys
end

function Builder:instance(name, comp)
	if not self.cur_sys then error("Unexpected 'instance'") end
    self.cur_node = self.cur_sys:add_node(name, comp)
end

function Builder:export(port, name)
	if not self.cur_sys then error("Unexpected 'export'") end
    self.cur_port = self.cur_sys:make_export(port, name)
end

function Builder:link(src, dest)
	if not self.cur_sys then error("Unexpected 'link'") end
    self.cur_sys:add_link(src, dest)
end

function Builder:make_exclusive()
end

function Builder:parameter(name, val)
    if not self.cur_node then error("Unexpected 'parameter'") end
    self.cur_node:def_param(name, val)
end

function Builder:signal(role, tag, vname, width)
    if not self.cur_port then error("Unexpected 'signal'") end
    
    local newargs
    
    if not vname then newargs = {role, tag, 1}
    elseif not width then newargs = {role, tag, vname}
    else newargs = {role, tag, vname, width}
    end
    
    self.cur_port:add_signal(table.unpack(newargs))
end

