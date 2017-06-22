require 'builder'

dofile 'cpu.lua'

b = genie.Builder.new()

define_cpu_parts(b, 1, false)
define_cpu(b, 1, true, 'cpu', false)