require 'builder'

dofile 'cpu.lua'

b = genie.Builder.new()

define_cpu(b, 1, 1, true, 'cpu')