require "util"

-- 
-- Builds a topology where everthing travels over one shared channel
--

function topo_shared_toob(sys)
	-- create a single split and a single merge node, and a link between them
    local split = sys:add_split('split')
    local merge = sys:add_merge('merge')
    local merge_to_split = sys:add_link(merge, split)
	
	-- go through all RS links
	for rslink in values(sys:get_links('rs')) do
        local src = rslink:get_src():get_topo_port()
        local sink = rslink:get_sink():get_topo_port()
        
        -- src to merge node
        local tplink = sys:add_link(src, merge:get_port('in'))
        
		-- connect split node to the final destination
		tplink = sys:add_link(split:get_port('out'), sink)
	end
end