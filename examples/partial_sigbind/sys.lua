require 'builder'
require 'topo_xbar'

local b = genie.Builder.new()

-- Partial signal binding example
-- The last parameter to signal() is usually just a bit width, but also has
-- an advanced mode where you provide an array containing three elements:
-- 1: The total width of the Verilog signal
-- 2: The actual number of bits to bind the signal to
-- 3: The least-significant bit index within the Verilog signal where the binding occurs

-- This example has three logical interfaces, but they all map to different ranges within the
-- same physical Verilog signal.

b:component('comp','comp')
    b:conduit_src('iface')
        b:signal('out', 'field', 'o_sig', 4)
        
b:system('sys', topo_xbar)
    b:conduit_src('iface1')
        b:signal('out', 'field', 'o_sig', {12, 4, 0})
    b:conduit_src('iface2')
        b:signal('out', 'field', 'o_sig', {12, 4, 4})
    b:conduit_src('iface3')
        b:signal('out', 'field', 'o_sig', {12, 4, 8})
    
    b:instance('c1', 'comp')
    b:instance('c2', 'comp')
    b:instance('c3', 'comp')

    b:link('c1.iface', 'iface1')
    b:link('c2.iface', 'iface2')
    b:link('c3.iface', 'iface3')
    
    