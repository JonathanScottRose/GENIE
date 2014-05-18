require 'spec'
require 'topo_xbar'
require 'topo_ring'

local s = spec.Spec:new()
local b = s:builder()

b:component('CpuComp', 'cpu')
  b:clock_sink('clk', 'i_clk')
  b:reset_sink('rst', 'i_reset')
  b:interface('out', 'data', 'out', 'clk')
    b:signal('valid', 'o_valid')
    b:signal('ready', 'i_ready')
    b:signal('data', 'o_data', '256')
    b:signal('lp_id', 'o_addr', '2')
    for i = 0, 3 do
      local nm = 'to_cpu' .. i
      local enc = "2'd" .. i
      b:linkpoint(nm, enc, 'broadcast')
    end
  b:interface('in', 'data', 'in', 'clk')
    b:signal('valid', 'i_valid')
    b:signal('ready', 'o_ready')
    b:signal('data', 'i_data', '256')
    b:signal('lp_id', 'i_addr', '2')
    for i = 0, 3 do
      local nm = 'to_cpu' .. i
      local enc = "2'd" .. i
      b:linkpoint(nm, enc, 'broadcast')
    end
	
b:system('fourcores', topo_ring)
  b:export('clk', 'clock', 'in')
  b:export('rst', 'reset', 'in')
  for i = 0, 3 do
    local me = 'cpu' .. i
    b:instance(me, 'CpuComp')
    b:link('clk', me .. '.clk')
    b:link('rst', me .. '.rst')
    for j = 0, 3 do
      local them = 'cpu' .. j
        b:link(me .. '.out.to_' .. them, 
               them .. '.in.to_' .. me)
      
    end
  end

s:submit()

