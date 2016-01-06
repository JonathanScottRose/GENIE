require 'builder'
require 'topo_ring'

local b = genie.Builder.new()

local rings = { {} }
local function rings_cb()
		local ring = {}
	for i = 0, 3 do
		table.insert(ring, 'cpu'..i..'.out')
	end
	for i = 0, 3 do
		table.insert(ring, 'cpu'..i..'.in')
	end
	rings = { ring }
	return rings
end

b:component('CpuComp', 'cpu')
  b:clock_sink('clk', 'i_clk')
  b:reset_sink('rst', 'i_reset')
  b:rs_src('out', 'clk')
    b:signal('valid', 'o_valid')
    b:signal('ready', 'i_ready')
    b:signal('data', 'o_data', '256')
    b:signal('lpid', 'o_addr', '2')
    for i = 0, 3 do
      local nm = 'to_cpu' .. i
      local enc = "2'd" .. i
      b:linkpoint(nm, enc, 'broadcast')
    end
  b:rs_sink('in', 'clk')
    b:signal('valid', 'i_valid')
    b:signal('ready', 'o_ready')
    b:signal('data', 'i_data', '256')
    b:signal('lpid', 'i_addr', '2')
    for i = 0, 3 do
      local nm = 'from_cpu' .. i
      local enc = "2'd" .. i
      lp = b:linkpoint(nm, enc, 'broadcast')
    end
	
b:system('fourcores', make_topo_ring(rings_cb))
  b:clock_sink('clk', 'clk')
  b:reset_sink('rst', 'rst')
  
  for i = 0, 3 do
	local me = 'cpu' .. i
	b:instance(me, 'CpuComp')
	b:link('clk', me .. '.clk')
    b:link('rst', me .. '.rst')
  end
  
  for i = 0, 3 do
    local me = 'cpu' .. i
    for j = 0, 3 do
	  if i ~= j then
		local them = 'cpu' .. j
        b:link(me .. '.out.to_' .. them, 
               them .. '.in.from_' .. me)
	  end
    end
  end


