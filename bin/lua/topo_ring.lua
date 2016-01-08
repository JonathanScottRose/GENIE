require 'util'
require 'topo_xbar'

function make_topo_ring(cb)
	if not cb or type(cb) ~= "function" then 
		error("make_topo_ring: Must provide a function that returns an array of networks, where each network is an array of interfaces") 
	end
	local rings = cb()
	
	-- this function, make_topo_ring, returns a topology function
	return function(sys)
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
		
		local uncon = sys:get_untopo_rs_links()
		local n_uncon = util.count(uncon)
		if n_uncon > 0 then
			print("topo_ring: found " .. n_uncon .. " RS links not belonging to a topology, passing to topo_xbar")
			for link in values(uncon) do
				print(" " .. link)
			end
			
			topo_xbar(sys)
		end
	end
end

