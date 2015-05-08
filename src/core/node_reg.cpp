#include "genie/node_reg.h"
#include "genie/net_clock.h"
#include "genie/net_reset.h"
#include "genie/net_rvd.h"
#include "genie/vlog_bind.h"

using namespace genie;
using namespace vlog;

namespace
{
	const std::string MODNAME = "genie_pipe_stage";
	const std::string INPORT_NAME = "in";
	const std::string OUTPORT_NAME = "out";
	const std::string CLOCKPORT_NAME = "clk";
	const std::string RESETPORT_NAME = "reset";
}

NodeReg::NodeReg()
	: Node()
{
	// Create verilog ports
	auto vinfo = new NodeVlogInfo(MODNAME);
	vinfo->add_port(new vlog::Port("i_clk", 1, vlog::Port::IN));
	vinfo->add_port(new vlog::Port("i_reset", 1, vlog::Port::IN));
	vinfo->add_port(new vlog::Port("i_data", "WIDTH", vlog::Port::IN));
	vinfo->add_port(new vlog::Port("i_valid", 1, vlog::Port::IN));
	vinfo->add_port(new vlog::Port("o_ready", 1, vlog::Port::OUT));
	vinfo->add_port(new vlog::Port("o_data", "WIDTH", vlog::Port::OUT));
	vinfo->add_port(new vlog::Port("o_valid", 1, vlog::Port::OUT));
	vinfo->add_port(new vlog::Port("i_ready", 1, vlog::Port::IN));
	set_hdl_info(vinfo);

	// Create genie ports and port bindings
	auto clkport = add_port(new ClockPort(Dir::IN, CLOCKPORT_NAME));
	clkport->add_role_binding(ClockPort::ROLE_CLOCK, new VlogStaticBinding("i_clk"));

	auto resetport = add_port(new ResetPort(Dir::IN, RESETPORT_NAME));
	resetport->add_role_binding(ResetPort::ROLE_RESET, new VlogStaticBinding("i_reset"));

	auto inport = new RVDPort(Dir::IN, INPORT_NAME);
	inport->set_clock_port_name(CLOCKPORT_NAME);
	inport->add_role_binding(RVDPort::ROLE_VALID, new VlogStaticBinding("i_valid"));
	inport->add_role_binding(RVDPort::ROLE_READY, new VlogStaticBinding("o_ready"));
	inport->add_role_binding(RVDPort::ROLE_DATA_CARRIER, new VlogStaticBinding("i_data"));
	inport->get_proto().set_carried_protocol(&m_proto);
	add_port(inport);

	auto outport = new RVDPort(Dir::OUT, OUTPORT_NAME);
	outport->set_clock_port_name(CLOCKPORT_NAME);
	outport->add_role_binding(RVDPort::ROLE_VALID, new VlogStaticBinding("o_valid"));
	outport->add_role_binding(RVDPort::ROLE_READY, new VlogStaticBinding("i_ready"));
	outport->add_role_binding(RVDPort::ROLE_DATA_CARRIER, new VlogStaticBinding("o_data"));
	outport->get_proto().set_carried_protocol(&m_proto);
	add_port(outport);

	auto link = (RVDInternalLink*)connect(inport, outport, NET_RVD_INTERNAL);
	link->set_latency(1);
}

RVDPort* NodeReg::get_input() const
{
	return as_a<RVDPort*>(get_port(INPORT_NAME));
}

RVDPort* NodeReg::get_output() const
{
	return as_a<RVDPort*>(get_port(OUTPORT_NAME));
}

HierObject* NodeReg::instantiate()
{
	throw HierException(this, "node not instantiable");
}

void NodeReg::do_post_carriage()
{
	define_param("WIDTH", m_proto.get_total_width());
}