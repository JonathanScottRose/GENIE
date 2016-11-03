--- Multi-Ring Topology.
-- Connects explicitly-provided RS ports in one or more rings.
-- Each RS output gets MERGEd onto the ring, and each RS input is fed by a SPLIT.
-- The function expects an array of rings, where
-- each ring is itself an array of RS ports. These can be specified directly by object
-- reference, or be strings representing hierarchical paths to the RS ports.
--@usage
--local ring1 = {'instA.iface1', 'instA.iface2', 'instB.someIface'}
--local ring2 = {'instA.iface3', 'instC.fooBarIface'}
--local rings = { ring1, ring2 }
--...
--local sys = b:system('mysys')
--...
--topo_ring(sys, rings)
-- @module topo_ring

require 'util'
require 'topo_xbar'

--- Multi-ring topology function. 
-- @tparam System sys @{System} to operate on
-- @tparam table rings an array of arrays which contain RS interface names/references
function topo_ring(sys, rings)
	-- build each ring network
	for ring in values(rings) do
		local ring_stops = {}
		
		-- go through each RS interface/linkpoint in the ring and create a split/merge node for it,
		-- saving it in ring_stops
		for iface in values(ring) do
			-- each entry provided by the user can either be an object or a string reference to the object.
			-- if it's the latter, convert it to the former.
			if type(iface) == 'string' then iface = sys:get_object(iface) end
			
			-- make sure this is an RS source/sink and get its topology endpoint
			if iface:get_type() ~= 'rs' then
				error("topo_ring: object " .. iface .. " is not an RS interface or linkpoint")
			end
			
			-- get the RS port's associated topology port
			local iface_topo = iface:get_topo_port()
			
			-- create a split or merge node depending on the user port's direction
			-- also make the connection
			local ringstop
			if iface_topo:get_dir() == 'out' then
				local nm = util.dot_to_uscore(util.con_cat(iface:get_hier_path(sys), 'mg'))
				ringstop = sys:add_merge(nm)
				sys:add_link(iface_topo, ringstop)
			else
				local nm = util.dot_to_uscore(util.con_cat(iface:get_hier_path(sys), 'sp'))
				ringstop = sys:add_split(nm)
				sys:add_link(ringstop, iface_topo)
			end
			
			table.insert(ring_stops, ringstop)
			
			-- add a reg node after each stop
			local reg = sys:add_buffer(util.con_cat(ringstop:get_name(), 'reg'))
			table.insert(ring_stops, reg)
		end
		
		-- connect the ringstops together
		for i = 1,#ring_stops do
			local j = (i % #ring_stops) + 1
			sys:add_link(ring_stops[i], ring_stops[j])
		end
	end
end

