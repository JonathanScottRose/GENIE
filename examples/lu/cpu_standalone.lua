require 'spec'
require 'topo_xbar'
require 'math' 

dofile 'cpu.lua'

-- Main entry point for GENIE to generate a single, standalone Compute Element that is not part of a greater LU
-- Factorization system.

s = spec.Spec:new()
b = s:builder()

define_cpu(b, 1, 1, true, 'cpu')

s:submit();