require 'builder'
require 'topo_shared_toob'

-- define serveral protocols, and create a sender/receiver pair for each one, with a link from the 
-- sender to the receiver. all links will share the same physical channel:
--
-- a_out \       /  a_in
-- ...   -M-----S-  ...
-- z_out /       \  z_in
--
-- All protocols will have valid/ready signals. The table below specifies the data signals and their
-- widths.

local protos =
{
	A =
	{
	},
	
	B = 
	{
		data = 16
	},
	
	C = 
	{
		x = 4,
		y = 5,
		z = 6,
		sop = 1,
		eop = 1
	},
	
	D =
	{
		data = 24,
		x = 4
	}
}

-- define sender/receivers components for each

local b = genie.Builder.new()

for name,sigs in spairsk(protos) do
	for dir in Set.mkvalues{"out", "in"} do
		local comp_name = name .. "_" .. dir
		local pfx_fwd = ((dir == "out") and "o_" or "i_")
		local pfx_rev = ((dir == "out") and "i_" or "o_")
		
		b:component(comp_name, comp_name)
		b:clock_sink('clk', 'clk')
		b:reset_sink('reset', 'reset')
		b:interface(dir, 'rs', dir)
            b:assoc_clk('clk')
            b:signal('valid', pfx_fwd .. 'valid')
            b:signal('ready', pfx_rev .. 'ready')
		for sname,swidth in pairs(sigs) do
			local stype
			local usertype = nil
			if sname == 'eop' or sname == 'data' then
				stype = sname
			else
				stype = 'databundle'
				usertype = sname
			end
			
            if usertype then
                b:signal(stype, usertype, pfx_fwd .. sname, swidth)
            else
                b:signal(stype, pfx_fwd .. sname, swidth)
            end
		end
	end
end

-- define system and instances and links

local sys = b:system('sys')

    b:clock_sink('clk', 'clk')
    b:reset_sink('reset', 'reset')

    for pname in keys(protos) do
        -- instantiate a sender/receiver instance for each protocol
        for dir in Set.mkvalues{"out", "in"} do
            local comp_name = pname .. "_" .. dir
            local inst_name = comp_name .. "_inst"
            b:instance(inst_name, comp_name)
            
            -- link clocks and resets to this instance
            b:link('clk', inst_name .. '.clk')
            b:link('reset', inst_name .. '.reset')
        end
        
        -- create a link from the sender to the receiver
        b:link(pname .. "_out_inst.out", pname .. "_in_inst.in")
    end
		

topo_shared_toob(sys)


