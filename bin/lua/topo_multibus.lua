require 'util'

--- Multi-Bus Topology.
-- Creates multiple shared buses, one per communication domain.
-- Two RS interfaces belong to the same domain if
-- there is a link between them or one of their linkpoints.
-- Each bus is unidirectional, and has all the sinks located
-- at the downstream end.
--@usage
--b:system('mysys', topo_multibus)
-- @module topo_multibus

function topo_multibus(sys)
	-- keep track of the members of each domain, and each RS port's domain membership
	local domains = {}
	local domain_of = {}
	
	-- build domains
	for link in values(sys:get_links('rs')) do
		local src = link:get_src():get_rs_port()
		local sink = link:get_sink():get_rs_port()
		
		local src_domain = domain_of[src]
		local sink_domain = domain_of[sink]
		
		-- find or create domain for src
		if not src_domain then
			src_domain = Set.make{src}
			domain_of[src] = src_domain
			Set.add(domains, src_domain)
		end
		
		-- domain of sink must equal domain of src.
		-- if sink already has a domain, assimilate its members
		-- into src domain.
		
		if sink_domain then
			for member in Set.values(sink_domain) do
				domain_of[member] = src_domain
				Set.add(src_domain, member)
			end
			Set.del(domains, sink_domain)
		end
		
		domain_of[sink] = src_domain
		Set.add(src_domain, sink)
	end
	
	for domain in keys(domains) do
		-- 'last' holds most-downstream-constructed portion of bus
		local last = nil
		local sinks = {}

		for rs_member in Set.values(domain) do
			member = rs_member:get_topo_port()
			
			if member:get_dir() == 'out' then
				if not last then
					last = member
				else
					-- sources get merge'd onto the bus, followed by a buffer
					local mg = sys:add_merge(util.con_cat(util.dot_to_uscore(member:get_hier_path(sys)), 'mg'))
					sys:add_link(last, mg)
					sys:add_link(member, mg)
					local rg = sys:add_buffer(util.con_cat(mg:get_name(), 'reg'))
					sys:add_link(mg, rg)
					last = rg
				end
			else
				-- sinks are skipped over and gathered for the end
				table.insert(sinks, member)
			end
		end
		
		-- the bus feeds a single split node that fans out to all sinks
		if #sinks > 1 then
			local sp = sys:add_split(util.unique_key(sys:get_objects(), 'split'))
			sys:add_link(last, sp)
			last = sp
		end
		
		for sink in values(sinks) do
			sys:add_link(last, sink)
		end
	end
end