# GENIE Manual

## Introduction

GENIE (_GEN_ eric _I_ nterconnect _E_ ngine) is a tool for hardware designers who target FPGAs. It generates complete systems containing the designer's modules connected with auto-generated interconnect. 

What makes GENIE different from vendor-supplied system generation tools like [Altera Qsys](https://www.altera.com/products/design-software/fpga-design/quartus-prime/quartus-ii-subscription-edition/qts-qsys.html) or [Xilinx IPI](http://www.xilinx.com/products/design-tools/vivado/integration.html) is the amount of interconnect customization available. GENIE's interconnect is lightweight enough for creating fine-grained systems that would otherwise be treated as opaque "IP Blocks" and wired by hand-written RTL in other tools. It is also capable of creating traditional, coarse-grained systems with customizable network topologies.

Rather than just another academic FPGA NoC architecture, we've aimed to create a useful tool for helping simplify the end-to-end process of creating real systems. Here are some of GENIE's current features:
* Support for multiple clock domains, with automatic area-optimized and topology-aware insertion of clock crossing logic.
* Hierarchical design
* Script-driven design flow with [Lua](http://www.lua.org)
* Multicast and broadcast-capable addressing
* Customizable network topology
* [Split/Merge](http://ieeexplore.ieee.org/xpls/abs_all.jsp?arnumber=6412110&tag=1)-based interconnect fabric with custom buffer placement
* Optimization hints to help exploit application-specific communication patterns

## Design Flow and Methodology

The input to GENIE is provided as an executable Lua script. Syntatcic sugar has been added to simplify the most common operations. The designer defines one or more _Systems_ containing instances of _Components_, which mimics the RTL paradigm of instantiating modules. Each _Component_ is a wrapper around a functional module that the user has written in Verilog, and describes its input/output ports, grouping them into _Interfaces_ with interconnect-related roles. Some Interfaces are for delivering clock/reset signals, but others are used for actually transmitting data.

GENIE uses a _Routed Streaming_ (RS) protocol for data-transmitting interfaces. This augments transmitted data signals with optional valid/ready handshaking signals and an optional addressing signal that supports unicast, multicast, and broadcast selection of destinations.

The user defines logical, end-to-end links between the Interfaces of instantiated Components, and chooses a _topology_ for the physical interconnect that GENIE generates. The final output is a SystemVerilog top-level module that instantiates both the designer's Components as well as GENIE interconnect primitives.

## Trivial Example

Here's a complete, minimal example that trivially connects two components together in a point-to-point fashion:

```language:lua
require 'builder'
require 'topo_xbar'

-- create a Builder object to interface with the GENIE API
b = genie.Builder.new()

-- begin definition of component compA which wraps Verilog module comp_a_ver
b:component('compA', 'comp_a_ver')	
	-- create a clock sink interface, containing Verilog input port i_clk
	b:clock_sink('clkIface', 'i_clk')
	-- create a Routed Streaming source interface, synchronous to clock interface clkIface
	b:rs_src('sender', 'clkIface')
		-- this Interface contains a 10 bit wide data signal and a valid signal for handshaking
		b:signal('data', 'o_data', 10)
		b:signal('valid', 'o_vld')

-- create a second similar component that has a Sink interface
b:component('compB', 'comp_b_ver')	
	b:clock_sink('clkIface', 'i_clk')
	b:rs_sink('recvr', 'clkIface')
		b:signal('data', 'i_data', 10)
		b:signal('valid', 'i_vld')
		
-- Define a top-level system called TheSys.
-- This system uses the built-in Sparse Crossbar topology function topo_xbar,
-- even though we expect a single point-to-point link.
b:system('TheSys', topo_xbar)
	-- Create a top-level clock sink interface for the system and
	-- decide what the name of the associated Verilog signal will be 
	-- in the GENIE-generated Verilog module that will be output.
	--
	-- Systems behave like Components in GENIE and are allowed 
	-- to have Interfaces too.
	b:clock_sink('TopLevelClk', 'i_clk200mhz')
	
	-- instantiate our two components once each
	b:instance('instA', 'compA')
	b:instance('instB', 'compB')
	
	-- connect clocks
	b:link('TopLevelClk', 'instA.clkIface')
	b:link('TopLevelClk', 'instB.clkIface')
	
	-- define the point-to-point data link
	b:link('instA.sender', 'instB.recvr')
```

This generates a file `TheSys.sv`, which looks like this:

```language:systemverilog
module TheSys
(
    input reset,
    input i_clk200mhz
);
    wire instA_o_vld;
    wire [9:0] instA_o_data;
    
    comp_a_ver instA
    (
        .i_clk(i_clk200mhz),
        .o_vld(instA_o_vld),
        .o_data(instA_o_data)
    );
    
    comp_b_ver instB
    (
        .i_clk(i_clk200mhz),
        .i_data(instA_o_data),
        .i_vld(instA_o_vld)
    );
    
endmodule
```

Note the auto-created reset signal, which ended being unused. Also note that for this trivial example, the amount of specification code exceeds the Verilog output! Let's try a more complex example.

## Slightly More Useful Example

Here's the same example but this time the system contains two instances of the B component, and A communicates with both of them.

```language:lua
require 'builder'
require 'topo_xbar'

b = genie.Builder.new()

-- make a constant for the data size
DWIDTH = 10

-- create two basic components
b:component('compA', 'comp_a_ver')	
	b:clock_sink('clkIface', 'i_clk')
	b:rs_src('sender', 'clkIface')
		b:signal('data', 'o_data', DWIDTH)
		b:signal('valid', 'o_vld')
			
b:component('compB', 'comp_b_ver')	
	b:clock_sink('clkIface', 'i_clk')
	b:rs_sink('recvr', 'clkIface')
		b:signal('data', 'i_data', DWIDTH)
		b:signal('valid', 'i_vld')
		
b:system('TheSys', topo_xbar)
	b:clock_sink('TopLevelClk', 'i_clk200mhz')
	
	-- instantiate our two components once each
	b:instance('instA', 'compA')
	b:instance('instB1', 'compB')
	b:instance('instB2', 'compB')
	
	-- connect clocks using a Lua for loop and string concatenation
	for _,dest in ipairs{'instA', 'instB1', 'instB2'} do
		b:link('TopLevelClk', dest .. '.clkIface')
	end
	
	-- define the point-to-point data link
	b:link('instA.sender', 'instB1.recvr')
	b:link('instA.sender', 'instB2.recvr')
```

What happens now? Well, instance A will broadcast to both instances of B through a Split Node, which is a piece of GENIE-generated interconnect. The actual hardware will optimize away to just wires. Here's the Verilog output:

```language:systemverilog
module TheSys
(
    input reset,
    input i_clk200mhz
);
    wire [9:0] split0_o_data;
    wire instA_o_vld;
    wire [1:0] split0_o_valid;
    wire [9:0] instA_o_data;
    
    comp_b_ver instB1
    (
        .i_clk(i_clk200mhz),
        .i_data(split0_o_data),
        .i_vld(split0_o_valid[1])
    );
    
    comp_a_ver instA
    (
        .i_clk(i_clk200mhz),
        .o_vld(instA_o_vld),
        .o_data(instA_o_data)
    );
    
    genie_split #
    (
        .FLOWS(1'b0),
        .NO(2),
        .NF(1),
        .WF(1),
        .ENABLES(2'b11),
        .WO(10)
    )
    split0
    (
        .o_data(split0_o_data),
        .clk(i_clk200mhz),
        .reset(reset),
        .i_flow(1'd0),
        .i_data(instA_o_data),
        .i_valid(instA_o_vld),
        .o_ready(),
        .o_flow(),
        .o_valid(split0_o_valid),
        .i_ready({1'd1,1'd1})
    );
    
    comp_b_ver instB2
    (
        .i_clk(i_clk200mhz),
        .i_data(split0_o_data),
        .i_vld(split0_o_valid[0])
    );
    
endmodule
```

## Addressing with Linkpoints

In the previous example, data was broadcast unconditionally to both destinations. What if instance A needs to choose a single destination to send to? This is accomplished using _Linkpoints_.

A Linkpoint is a virtual connection point associated with a Routed Streaming Interface. It has a name, and a _Linkpoint ID_, which the Component drives on an ``lpid`` signal of the Interface to select the Linkpoint when transmitting. Links can now be defined between Linkpoints and remote RS Interfaces. When a Linkpoint is selected with the appropriate LPID signal, all destination interfaces connected _to that linkpoint_ are send the data.

Sink interfaces are allowed to define Linkpoints as well, and this can be used to discern the source of incoming traffic. The previous example has been modified in the code fragment below to allow component A to use Linkpoints:

```language:lua

...
		-- (within definition of Component A)
		b:rs_src('sender', 'clkIface')
			b:signal('data', 'o_data', DWIDTH)
			b:signal('valid', 'o_vld')
			-- new: create a 1-bit wide Linkpoint ID signal and
			-- define two linkpoints
			b:signal('lpid', 'o_dest', 1)
			b:linkpoint('lpA', "1'b0")
			b:linkpoint('lpB', "1'b1")
...
...
	-- define data links between A's linkpoints and the two B interfaces
	b:link('instA.sender.lpA', 'instB1.recvr')
	b:link('instA.sender.lpB', 'instB2.recvr')
...
```

Note the hierarchical syntax of selecting Linkpoints `lpA` and `lpB` when creating end-to-end links.

Now, Component A can talk to B1 by driving `o_dest` to 0 and talk to B2 by driving `o_dest` to 1. We could even create a third Linkpoint (after increasing the LPID width to 2) with encoding `2'b10` or `2'b11` that still is able to broadcast to both destinations simultaneously.

Internally, GENIE inserts Flow Converters (`genie_flowconv`) into the generated system to convert between the Component-understood Linkpoint IDs and an internal Flow ID that uniquely identifies each end-to-end transmission. This allows Split nodes along the way to perform routing decisions.

## Exporting Interfaces

Systems behave like Components - they can be instantiated in another System, and they also have Interfaces. It is possible to manually create such Interfaces (even going as far as providing the names of the Verilog signals that will be used within one). This was the method used for creating top-level clock sources in the examples previously.

However, sometimes all you want to do is export an Interface of some Component to the outside world, creating the correct type (and matching direction) of Interface and connecting them together with a single Link. The `export` command lets you do this.

```language:lua
	b:export('instB1.recvr', 'TopLevelRecvr')
```

which is equivalent to:
```language:lua
	b:rs_sink('TopLevelRecvr', 'TopLevelClk')
	b:link('TopLevelRecvr', 'instB1.recvr')
```

Note that `export` figures out the correct associated clock to use, as well as the type and direction of the new Interface. Any interface may be exported. With RS Ports, the Linkpoint definitions are copied to the exported interface. Had we instead exported `instA.sender`:

```language:lua
	b:export('instA.sender', 'TopLevelSender')
```

it would have been equivalent to creating matching Linkpoints and explicitly forwarding each one:
```language:lua
	b:rs_src('TopLevelSender', 'TopLevelClk')
		b:linkpoint('lpA', "1'b0")
		b:linkpoint('lpB', "1'b1")
	b:link('instA.sender.lpA', 'TopLevelSender.lpA')
	b:link('instA.sender.lpB', 'TopLevelSender.lpB')
```

## Interface Types ##

| Type | Description |
|------|-----------------------------------|
| clock | Sends/receives clock signals. Each clock interface contains a single signal of type 'clock'. Also used to associate a clock domain with an RS Interface |
| reset | For sending/receiving reset signals. Contains a single signal of type 'reset' |
| conduit | A catch-all bundle of signals that are not subject to interconnect synthesis. Connecting two conduit interfaces connects their signals with wires. This is useful for exporting the input/output/bidirectional signals of off-chip memory controllers or other hardware without any complicated interconnect. |
| rs | Routed Streaming Interfaces are used to carry data over a GENIE-generated network. Links made between RS interfaces are routed over a physical topology defined during System creation time. RS Interfaces can have Linkpoints defined within them for addressing purposes |

## Signal Roles ##

Each Interface type supports different types of contained signals. 

The direction of each signal depends on the direction of the containing Interface. Some signals go in the same direction as their interface, and these are labeled _fwd_ in the table below. For example, a _valid_ signal on a Sink RS Interface will need to be an `input` port in the Component's Verilog module. Others, such as _ready_ signals, are labeled _reverse_ and go in the opposite direction as their Interface. Conduit Interfaces offer special roles that have explicit input/output/bidir directions independent of their Interface.

Usually, a maximum of one signal of each role is permitted per Interface. A clock Interface can only have one _clock_ signal, and an RS Interface can only have one _valid_ signal. However, some types of signals roles can appear more than once, and must be differentiated with a user-provided _tag_ which is just a unique string. 

For example, a Conduit can hold a bundle of input, output, and bidirectional signals. When two Conduits are connected, signals with matching tags are paired up and connected. The same holds for RS interfaces: while they allow only a single _data_ signal, it is possible to have many named _databundle_ signals with different tags.


| Interface Type | Signal Role | Width | Direction | Tag Required? | Description |
|----------------|-------------|-------|-----------|---------------|-------------|
| clock          | clock       | 1     | forward   | no            | Clock signal |
| reset          | reset       | 1     | forward   | no            | Reset signal |
| conduit        | fwd         | _user-defined_ | forward | yes    | Arbitrary signals, going with or against the direction of the parent Interface. Each must have a unique user-defined string tag. |
|                | rev         |       | reverse   |        |                     |
|                | in          |       | input     | yes    | Arbitrary signals with explicit input/output/bidir direction, independent of parent Interface's direction |
|                | out         |       | output    | yes    | |
|                | bidir       |       | inout     | yes    | |
| rs             | data        | _user-defined_ | forward | no | Single data signal |
|                | databundle  | _user-defined_ | forward | yes | Used for multiple, named data signals |
|                | valid       | 1     | forward   | no  | Indicates data is valid this cycle |
|                | ready       | 1     | reverse   | no  | High when ready to receive data |
|                | lpid        | _user-defined_ | forward | no | Linkpoint ID, used for selecting or being informed of the Linkpoint being sent/received on this cycle |
|                | eop         | 1     | forward | no | End-of-Packet. When transmitting a block of data over many cycles, keeping this signal unasserted until the last cycle will ensure that it will be received contiguously at the destination without being mixed with transmissions from other sources. |

## RS Interface Signals

There are many allowed signal roles, but not all are necessary. In fact, even _data_ signals are optional, as long as there's a _valid_ present. This can be used to signal an event to a destination without associated data. Here's a list of validity rules:

| Signal | Necessary? |
|--------|-------------|
| data/databundle | No, as long as there's a _valid_ signal present |
| valid | No, unless there are no data/databundle signals present |
| ready | No |
| lpid | Only when the Interface has Linkpoints |
| eop | No |

There may be additional restrictions that depend on the nature of the connectivity of the system. An RS Source with _valid_ needs to communicate with Sinks that also have _valid_, otherwise data could get lost. The opposite is not true: an RS Source with no _valid_ is just assumed to be sending valid data continously, so a connected RS Sink with a _valid_ will just receive 1's all the time.

When two sources are feeding a sink, only one can physically be allowed to reach the sink at any time. This necessitates the the sources support a _ready_ signal to know when to back off.

## Parameters

GENIE supports the parameterization of Components with Verilog Parameters. They can be passed down into the associated Verilog module during Component instantiation, or be used for signal widths. Here's an example of a Component that has parameterizable width:

```language:lua
b:component('Inverter', 'inverter')
	b:parameter('WIDTH')
	b:clock_sink('Clk', 'clk')
	b:rs_sink('Input', 'Clk')
		b:signal('data', 'i_in', 'WIDTH')
	b:rs_src('Output', 'Clk')
		b:signal('data', 'o_out', 'WIDTH')

b:system('sys', topo_xbar)
	b:instance('Inverter11', 'Inverter')
	b:parameter('WIDTH', 16)

	b:instance('Inverter2', 'Inverter')
	b:parameter('WIDTH', 20)
```

The `parameter` function is used both to define parameters on Components, and to set them on instances. It applies to the object created by the most recent `component`, `instance`, or `system` command. Parameters can be referenced when specifying signal widths. 

Parameters are passed to the Verilog modules during instantiation:

```language:systemverilog

inverter #
(
	.WIDTH(16)
)
Inverter1
(
	input clk,
	input [WIDTH-1:0] i_in,
	output [WIDTH-1:0] o_out
)
...
```

Systems can be parameterized too, and those parameters can be referenced by the System's top-level Interfaces or be passed down to Component instances:

```language:lua
b:component('Inverter', 'inverter')
	b:parameter('WIDTH')
	b:clock_sink('Clk', 'clk')
	b:rs_sink('Input', 'Clk')
		b:signal('data', 'i_in', 'WIDTH')
	b:rs_src('Output', 'Clk')
		b:signal('data', 'o_out', 'WIDTH')

b:system('sys', topo_xbar)
	b:parameter('SYSWIDTH')
	
	b:instance('Inverter11', 'Inverter')
	b:parameter('WIDTH', 'SYSWIDTH')
```

## Expressions

Parameters can be referenced in mathematical expressions. GENIE supports the following operations:

| Operator | Description |
|----------|-------------|
| +        | Addition | 
| -        | Subtraction | 
| *        | Multiplication | 
| /        | Division | 
| %        | Ceiling of log base 2, unary |

`%` has highest precedence, followed by multiplication/division followed by addition/subtraction. Parameters can appear in expressions. Every reference to a parameter is actually just a trivial expression with no operators. Here are two uses of expressions:

```language:lua
-- when passing parameters:
b:system(...)
b:parameter('SYSPARAM')
...
b:instance(...)
b:parameter('INSTPARAM', 'SYSPARAM*2 -1')
```

```language:lua
--- when specifying signal widths:
b:rs_sink (...)
	b:signal('data', 'i_data', '%WIDTH - 1')
```

Uses of `%` are converted to calls to the SystemVerilog built-in `$clog2` system function.

## Creating Custom Topologies

When creating a System, it is necessary to provide a topology function. A common one is `topo_xbar` which is GENIE's Sparse Crossbar topology. This function is Lua code that is executed after the main script. Its responsibility is to:
* create Split, Merge, and Buffer nodes
* make connections between them, and to source/sink RS Interfaces in the design

The system's RS links are automatically routed over the network using a shortest-hop algorithm.

A topology function is a regular Lua function that accepts one parameter: the system being processed. The function thus has access to all objects inside the System and can use that information to generate a topology in a generic way. The source for `topo_xbar` provides an example of how to do this.

## Latency Queries

## Communication Hints