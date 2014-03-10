require "util"
require "spec_sys"
require "spec_comp"

-- Create this package and set up environment

if not spec then spec = {} end
setmetatable(spec, {__index = _ENV})
_ENV = spec

-- Spec

Spec = class
{
	components = {},
	systems = {}
}

function Spec:new(o)
	o = Spec:_init_inst(o)
	o.components = {}
	o.systems = {}
	return o
end

function Spec:add_component(comp)
	util.insert_unique(comp, self.components)
	return comp
end

function Spec:add_system(sys)
	sys.parent = self
	util.insert_unique(sys, self.systems)
	return sys
end

function Spec:builder()
	local result = Builder:new()
	result.cur_spec = self
	return result
end

function Spec:post_process()
	for component in values(self.components) do
		component:create_default_linkpoints()
	end
	
	for system in svaluesv(self.systems, function(a,b) return a:is_subsystem_of(b) end) do
		system:create_auto_exports()
		system:componentize()
		system:create_default_reset()
	end
	
	for component in values(self.components) do
		component:create_default_linkpoints()
	end
end

function Spec:submit()
	self:post_process()

	local function trans_iftype(type, dir)
		type = string.lower(type)
		dir = string.lower(dir)
		if type=="clock" and dir=="in" then return 'clock_sink' end
		if type=='clock' and dir=='out' then return 'clock_src' end
		if type=='reset' and dir=='in' then return 'reset_sink' end
		if type=='reset' and dir=='out' then return 'reset_src' end
		if type=='data' and dir=='in' then return 'recv' end
		if type=='data' and dir=='out' then return 'send' end
		if type=='conduit' then return 'conduit' end
	end
	
	for component in svaluesk(self.components) do
		ct.reg_component
		{
			name = component.name,
			hier = component.module
		}
		
		for iface in svaluesk(component.interfaces) do
			ct.reg_interface(component.name,
			{
				name = iface.name,
				type = trans_iftype(iface.type, iface.dir),
				clock = iface.clock
			})
			
			for sig in svaluesk(iface.signals) do
				ct.reg_signal(component.name, iface.name,
				{
					binding = sig.binding,
					type = sig.type,
					width = sig.width,
					usertype = sig.usertype
				})
			end
			
			for lp in svaluesk(iface.linkpoints) do
				ct.reg_linkpoint(component.name, iface.name,
				{
					name = lp.name,
					type = lp.type,
					encoding = lp.encoding
				})
			end
		end
	end
	
	for sys in svaluesk(self.systems) do
		ct.reg_system(sys.name)
		
		for obj in svaluesk(sys.objects) do
			if obj.type == "INSTANCE" then
				ct.reg_instance(sys.name,
				{
					name = obj.name,
					comp = obj.component
				})
				
				ct.inst_defparams(sys.name, obj.name, obj.param_bindings)
			else
				ct.reg_export(sys.name,
				{
					name = obj.name,
					type = trans_iftype(obj.iface_type, obj.iface_dir)
				})					
			end
		end
		
		for link in svaluesk(sys.links) do
			ct.reg_link(sys.name, link.src:str(), link.dest:str())
		end
		
		if not sys.topo_func then
			util.error("System " .. sys.name .. " missing topology function")
		end
		
		local g = sys.topo_func(sys)
		--g:dump_dot('topo.dot')
		g:submit()
	end	
end

-- Builder

Builder = class
{
	cur_spec = nil,
	cur_comp = nil,
	cur_iface = nil,
	cur_lp = nil,
	cur_sys = nil,
	cur_inst = nil
}

function Builder:new(spec)
	local result = Builder:_init_inst {spec = spec}
	return result
end

function Builder:component(name, module)
	self.cur_comp = Component:new
	{
		name = name,
		module = module
	}
	self.cur_spec:add_component(self.cur_comp)
end

function Builder:parameter(name, width, depth)
	if not self.cur_comp then util.errlr("Unexpected 'parameter'") end
	self.cur_comp:add_parameter(Parameter:new
	{
		name = name,
		width = width or 1,
		depth = depth or 1
	})
end

function Builder:interface(name, type, dir, clock)
	if not self.cur_comp then util.error("Unexpected 'interface'") end
	self.cur_iface = Interface:new
	{
		name = name,
		type = string.upper(type),
		dir = string.upper(dir),
		clock = clock
	}
	self.cur_comp:add_interface(self.cur_iface)
end

function Builder:clock_sink(name, binding)
	self:interface(name, "clock", "in")
	self:signal("clock", binding, 1)
end

function Builder:reset_sink(name, binding)
	self:interface(name, "reset", "in")
	self:signal("reset", binding, 1)
end

function Builder:signal(type, binding, width, usertype)
	if not self.cur_iface then util.error("Unexpected 'signal'") end
	
	type = string.upper(type)
	local monowidth = Set.make {"CLOCK", "RESET", "VALID", "READY", "SOP", "EOP"}
	if width == nil then
		if monowidth[type] then
			width = 1
		else
			util.error("Signal " .. binding .. " missing width")
		end
	end
	
	self.cur_iface:add_signal(Signal:new
	{
		type = type,
		binding = binding,
		width = width,
		usertype = usertype
	})
end

function Builder:linkpoint(name, encoding, type)
	if not self.cur_iface then util.error("Unexpected 'linkpoint'") end
	self.cur_iface:add_linkpoint(Linkpoint:new
	{
		name = name,
		encoding = encoding,
		type = string.upper(type)
	})
end

function Builder:system(name, topofunc)
	self.cur_sys = System:new
	{
		name = name,
		topo_func = topofunc
	}
	self.cur_spec:add_system(self.cur_sys)
end

function Builder:instance(name, comp)
	if not self.cur_sys then util.error("Unexpected 'instance'") end
	self.cur_inst = Instance:new
	{
		name = name,
		component = comp
	}
	self.cur_sys:add_object(self.cur_inst)
end

function Builder:export(name, type, dir)
	if not self.cur_sys then util.error("Unexpected 'export'") end
	local export = Export:new
	{
		name = name,
		iface_type = string.upper(type),
		iface_dir = string.upper(dir)
	}
	self.cur_sys:add_object(export)
end

function Builder:defparam(name, value)
	if not self.cur_inst then util.error("Unexpected 'defparam'") end
	self.cur_inst:bind_param(name, value)
end

function Builder:link(src, dest)
	if not self.cur_sys then util.error("Unexpected 'link'") end
	local link = Link:new
	{
		src = LinkTarget:new(),
		dest = LinkTarget:new()
	}
	link.src:parse(src)
	link.dest:parse(dest)
	self.cur_sys:add_link(link)
end
