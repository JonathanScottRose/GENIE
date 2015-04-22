#include "genie/node_merge.h"
#include "genie/net_rvd.h"
#include "genie/net_clock.h"
#include "genie/net_reset.h"
#include "genie/net_topo.h"
#include "genie/genie.h"
#include "genie/vlog_bind.h"

using namespace genie;
using namespace vlog;

namespace
{
	const std::string MODULE = "genie_merge";
	const std::string MODULE_EX = "genie_merge_ex";

	const std::string INPORT_NAME = "in";
	const std::string OUTPORT_NAME = "out";
	const std::string CLOCKPORT_NAME = "clock";
	const std::string RESETPORT_NAME = "reset";
}

void NodeMerge::init_vlog()
{
	using namespace vlog;

	auto vinfo = new NodeVlogInfo(m_exclusive? MODULE_EX : MODULE);

	vinfo->add_port(new vlog::Port("clk", 1, vlog::Port::IN));
	vinfo->add_port(new vlog::Port("reset", 1, vlog::Port::IN));
	vinfo->add_port(new vlog::Port("i_data", "NI*WIDTH", vlog::Port::IN));
	vinfo->add_port(new vlog::Port("i_valid", "NI", vlog::Port::IN));
	vinfo->add_port(new vlog::Port("i_eop", "NI", vlog::Port::IN));
	vinfo->add_port(new vlog::Port("o_ready", "NI", vlog::Port::OUT));
	vinfo->add_port(new vlog::Port("o_valid", 1, vlog::Port::OUT));
	vinfo->add_port(new vlog::Port("o_eop", 1, vlog::Port::OUT));
	vinfo->add_port(new vlog::Port("o_data", "WIDTH", vlog::Port::OUT));
	vinfo->add_port(new vlog::Port("i_ready", 1, vlog::Port::IN));

	set_hdl_info(vinfo);
}

NodeMerge::NodeMerge()
{	
	init_vlog();

	// Clock and reset ports are straightforward
	auto port = add_port(new ClockPort(Dir::IN, CLOCKPORT_NAME));
	port->add_role_binding(ClockPort::ROLE_CLOCK, new VlogStaticBinding("clk"));

	port = add_port(new ResetPort(Dir::IN, RESETPORT_NAME));
	port->add_role_binding(ResetPort::ROLE_RESET, new VlogStaticBinding("reset"));

	// Input port and output port start out as Topo ports
	add_port(new TopoPort(Dir::IN, INPORT_NAME));
	add_port(new TopoPort(Dir::OUT, OUTPORT_NAME));
}

NodeMerge::~NodeMerge()
{
}

TopoPort* NodeMerge::get_topo_input() const
{
	return as_a<TopoPort*>(get_port(INPORT_NAME));
}

TopoPort* NodeMerge::get_topo_output() const
{
	return as_a<TopoPort*>(get_port(OUTPORT_NAME));
}

int NodeMerge::get_n_inputs() const
{
	return get_topo_input()->get_n_rvd_ports();
}

RVDPort* NodeMerge::get_rvd_output() const
{
	return get_topo_output()->get_rvd_port();
}

RVDPort* NodeMerge::get_rvd_input(int idx) const
{
	return get_topo_input()->get_rvd_port(idx);
}

void NodeMerge::refine(NetType target)
{
	// Default
	HierObject::refine(target);

	if (target == NET_RVD)
	{
		// Make bindings for the RVD ports
		auto port = get_rvd_output();
		port->set_clock_port_name(CLOCKPORT_NAME);
		port->add_role_binding(RVDPort::ROLE_VALID, new VlogStaticBinding("o_valid"));
		port->add_role_binding(RVDPort::ROLE_READY, new VlogStaticBinding("i_ready"));
	
		int n = get_n_inputs();
		define_param("NI", n);

		for (int i = 0; i < get_n_inputs(); i++)
		{
			auto port = get_rvd_input(i);

			port->set_clock_port_name(CLOCKPORT_NAME);
			port->add_role_binding(RVDPort::ROLE_VALID, new VlogStaticBinding("i_valid", 1, i));
			port->add_role_binding(RVDPort::ROLE_READY, new VlogStaticBinding("o_ready", 1, i));
		}
	}
}

HierObject* NodeMerge::instantiate()
{
	throw HierException(this, "node not instantiable");
}