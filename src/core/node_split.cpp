#include "genie/node_split.h"
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
	const std::string MODULE = "genie_split";

	const std::string INPORT_NAME = "in";
	const std::string OUTPORT_NAME = "out";
	const std::string CLOCKPORT_NAME = "clock";
	const std::string RESETPORT_NAME = "reset";
}

void NodeSplit::init_vlog()
{
	auto vinfo = new NodeVlogInfo(MODULE);

	vinfo->add_port(new vlog::Port("clk",  1, vlog::Port::IN));
	vinfo->add_port(new vlog::Port("reset",  1, vlog::Port::IN));
	vinfo->add_port(new vlog::Port("i_data",  "WO", vlog::Port::IN));
	vinfo->add_port(new vlog::Port("i_flow",  "WF", vlog::Port::IN));
	vinfo->add_port(new vlog::Port("i_valid",  1, vlog::Port::IN));
	vinfo->add_port(new vlog::Port("o_ready",  1, vlog::Port::OUT));
	vinfo->add_port(new vlog::Port("o_valid",  "NO", vlog::Port::OUT));
	vinfo->add_port(new vlog::Port("o_data",  "WO", vlog::Port::OUT));
	vinfo->add_port(new vlog::Port("o_flow",  "WF", vlog::Port::OUT));
	vinfo->add_port(new vlog::Port("i_ready",  "NO", vlog::Port::IN));

	set_hdl_info(vinfo);
}

NodeSplit::NodeSplit()
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

NodeSplit::~NodeSplit()
{
}

TopoPort* NodeSplit::get_topo_input() const
{
	return as_a<TopoPort*>(get_port(INPORT_NAME));
}

TopoPort* NodeSplit::get_topo_output() const
{
	return as_a<TopoPort*>(get_port(OUTPORT_NAME));
}


int NodeSplit::get_n_outputs() const
{
	return get_topo_output()->get_n_rvd_ports();
}

RVDPort* NodeSplit::get_rvd_input() const
{
	return get_topo_input()->get_rvd_port();
}

RVDPort* NodeSplit::get_rvd_output(int idx) const
{
	return get_topo_output()->get_rvd_port(idx);
}

void NodeSplit::refine(NetType target)
{
	// Do the default. TOPO ports will get the correct number of RVD subports.
	HierObject::refine(target);

	if (target == NET_RVD)
	{
		// Make bindings for the RVD ports
		auto port = get_rvd_input();
		port->set_clock_port_name(CLOCKPORT_NAME);
		port->add_role_binding(RVDPort::ROLE_VALID, new VlogStaticBinding("i_valid"));
		port->add_role_binding(RVDPort::ROLE_READY, new VlogStaticBinding("o_ready"));
	
		int n = get_n_outputs();
		define_param("NO", n);

		for (int i = 0; i < n; i++)
		{
			auto port = get_rvd_output(i);

			port->set_clock_port_name(CLOCKPORT_NAME);
			port->add_role_binding(RVDPort::ROLE_VALID, new VlogStaticBinding("o_valid", 1, i));
			port->add_role_binding(RVDPort::ROLE_READY, new VlogStaticBinding("i_ready", 1, i));
		}
	}
}

HierObject* NodeSplit::instantiate()
{
	throw HierException(this, "not instantiable");
}