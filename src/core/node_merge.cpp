#include "genie/node_merge.h"
#include "genie/net_rvd.h"
#include "genie/net_clock.h"
#include "genie/net_reset.h"
#include "genie/net_topo.h"
#include "genie/genie.h"
#include "genie/vlog_bind.h"

using namespace genie;

namespace
{
	const std::string MODULE = "genie_merge";
	const std::string MODULE_EX = "genie_merge_ex";

	const std::string INPORT_NAME = "in";
	const std::string OUTPORT_NAME = "out";
	const std::string CLOCKPORT_NAME = "clock";
	const std::string RESETPORT_NAME = "reset";
}

void NodeMerge::init()
{
	using namespace vlog;

	// Register vlog modules
	for (auto& name : {MODULE, MODULE_EX})
	{
		auto mod = new Module(name);

		mod->add_port(new vlog::Port("clk", 1, vlog::Port::IN));
		mod->add_port(new vlog::Port("reset", 1, vlog::Port::IN));
		mod->add_port(new vlog::Port("i_data", "NI*WIDTH", vlog::Port::IN));
		mod->add_port(new vlog::Port("i_valid", "NI", vlog::Port::IN));
		mod->add_port(new vlog::Port("i_eop", "NI", vlog::Port::IN));
		mod->add_port(new vlog::Port("o_ready", "NI", vlog::Port::OUT));
		mod->add_port(new vlog::Port("o_valid", 1, vlog::Port::OUT));
		mod->add_port(new vlog::Port("o_eop", 1, vlog::Port::OUT));
		mod->add_port(new vlog::Port("o_data", "WIDTH", vlog::Port::OUT));
		mod->add_port(new vlog::Port("i_ready", 1, vlog::Port::IN));

		vlog::register_module(mod);
	}
}

NodeMerge::NodeMerge()
{	
	using namespace vlog;

	asp_add(new AVlogInfo())->set_module(vlog::get_module(MODULE));

	// Clock and reset ports are straightforward
	add_port(new ClockPort(Dir::IN, CLOCKPORT_NAME));
	add_port(new ResetPort(Dir::IN, RESETPORT_NAME));

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
	return get_topo_output()->get_rvd_port(idx);
}

void NodeMerge::refine(NetType target)
{
	// Default
	HierObject::refine(target);
}