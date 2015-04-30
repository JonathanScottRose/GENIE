require "util"

-- 
-- Builds a topology where everthing travels over one shared channel
--

function topo_shared_toob(sys)
	-- create a single split and a single merge node, and a link between them
    local split = sys:add_split('split')
    local merge = sys:add_merge('merge')
    local merge_to_split = sys:add_link(merge:get_port('out'), split:get_port('in'))
	
	-- go through all RS links
	for rslink in values(sys:get_links('rs')) do
        local src = rslink:get_src():get_topo_port()
        local sink = rslink:get_sink():get_topo_port()
        
        -- src to merge node
        local tplink = sys:add_link(src, merge:get_port('in'))
        tplink:add_parent(rslink)
        
        -- make merge to split tplink carry the rslink
        merge_to_split:add_parent(rslink)
			
		-- connect split node to the final destination
		tplink = sys:add_link(split:get_port('out'), sink)
		tplink:add_parent(rslink)
	end
end