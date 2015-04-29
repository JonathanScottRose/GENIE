require "util"

-- Builder

genie.Builder = class()
local g = genie;
local Builder = genie.Builder

-- Private functions for looking up links by their labels
local function name_to_link(self, sys, name)
    local t = self.name2link[sys]
    local link = t[name]
    if not link then
        error("no link found with name " .. name .. " in system " .. sys)
    end
    return link
end
    
local function names_to_links(self, sys, names)
    local result = {}
    for name in values(names) do
        local link = name_to_link(self, sys, name)
        table.insert(result, link)
    end
    return result
end

function Builder:__ctor()
    -- Holds string->Link mappings for each System
    self.name2link = {}
end       

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
    
    -- initialize name2link table for this system
    self.name2link[self.cur_sys] = {}
end

function Builder:instance(name, comp)
	if not self.cur_sys then error("Unexpected 'instance'") end
    self.cur_node = self.cur_sys:add_node(name, comp)
end

function Builder:export(port, name)
	if not self.cur_sys then error("Unexpected 'export'") end
    self.cur_port = self.cur_sys:make_export(port, name)
end

function Builder:link(src, dest, label)
	if not self.cur_sys then error("Unexpected 'link'") end
    local link = self.cur_sys:add_link(src, dest)    
    local n2l = self.name2link[self.cur_sys]
    if label then 
        if n2l[label] then
            error('link with label ' .. label .. ' already exists ' .. 
                ' in system ' .. self.cur_sys)
        end
        n2l[label] = link 
    end
    return link
end

function Builder:make_exclusive(s)
    if not self.cur_sys then error("No current system defined") end

    -- ignore null args
    if not s then return end;
    
    -- ensure a table, and ignore empty table
    if type(s) ~= 'table' then 
        error('expected array/set of labels or RS link objects')
    end
    
    if not next(s) then return end
    
    -- based on the first entry in the table, guess whether or not we have
    -- been given a list of actual objects, or a list of string labels
    local k0,v0 = next(s)
    if type(k0) == 'string' or type(v0) == 'string' then
         s = names_to_links(self, self.cur_sys, s)
    elseif type(k0) ~= 'userdata' and type(v0) ~= 'userdata' then
        error('expected array/set of labels or RS link objects')
    end
    
    -- s is now for sure a set of link objects
    g.make_exclusive(s)    
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

function Builder:latency_query(link, param)
    if not self.cur_sys then error("No current system defined") end
    
    -- function overload: link may be a label, or an actual link.
    -- convert to an actual link if it's just a label
    if type(link) == 'string' then
        link = name_to_link(self, self.cur_sys, link)
    elseif type(link ~= 'userdata') then
        error('expected a link object or a link label')
    end
    
    self.cur_sys:create_latency_query(link, param)
end

