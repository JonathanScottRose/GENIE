require 'builder'
require 'topo_xbar'

local b = genie.Builder.new()

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
    
    