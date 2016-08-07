#include "genie/node_reg.h"
#include "genie/net_clock.h"
#include "genie/net_reset.h"
#include "genie/net_rvd.h"
#include "genie/net_topo.h"
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

NodeReg::NodeReg(bool notopo)
	: Node(), m_notopo(notopo)
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

    // Create clock and reset ports
    auto clkport = add_port(new ClockPort(Dir::IN, CLOCKPORT_NAME));
    clkport->add_role_binding(ClockPort::ROLE_CLOCK, new VlogStaticBinding("i_clk"));

    auto resetport = add_port(new ResetPort(Dir::IN, RESETPORT_NAME));
    resetport->add_role_binding(ResetPort::ROLE_RESET, new VlogStaticBinding("i_reset"));

    if (!notopo)
    {
	    // Create TOPO ports
	    // Input port and output port start out as Topo ports
	    auto inport = add_port(new TopoPort(Dir::IN, INPORT_NAME));
	    auto outport = add_port(new TopoPort(Dir::OUT, OUTPORT_NAME));
        auto link = (TopoLink*)connect(inport, outport);
        link->set_latency(1);
    }
    else
    {
        create_rvd();
    }
}

void NodeReg::refine(NetType type)
{
	// TOPO ports will get the correct number of RVD subports.
	HierObject::refine(type);

    if (type == NET_RVD)
    {
        create_rvd();
    }
}

TopoPort* NodeReg::get_topo_input() const
{
	return m_notopo? nullptr : as_a<TopoPort*>(get_port(INPORT_NAME));
}

TopoPort* NodeReg::get_topo_output() const
{
	return m_notopo? nullptr : as_a<TopoPort*>(get_port(OUTPORT_NAME));
}

RVDPort* NodeReg::get_input() const
{
	return m_notopo?
        as_a<RVDPort*>(get_port(INPORT_NAME)) :
        get_topo_input()->get_rvd_port();
}

RVDPort* NodeReg::get_output() const
{
    return m_notopo?
        as_a<RVDPort*>(get_port(OUTPORT_NAME)) :
        get_topo_output()->get_rvd_port();
}

HierObject* NodeReg::instantiate()
{
	throw HierException(this, "node not instantiable");
}

void NodeReg::do_post_carriage()
{
	define_param("WIDTH", m_proto.get_total_width());
}

genie::Port* NodeReg::locate_port(Dir dir, NetType type)
{
	// We accept TOPO connections directed at the node itself. This resolves
	// to either the input or output TOPO ports depending on dir.

    if (type == NET_RVD || (type == NET_INVALID && m_notopo))
    {
        if (dir == Dir::IN) return get_input();
        else if (dir == Dir::OUT) return get_output();
    }
    else if (type == NET_TOPO || (type == NET_INVALID && !m_notopo))
    {
        if (dir == Dir::IN) return get_topo_input();
        else if (dir == Dir::OUT) return get_topo_output();
    }

    return HierObject::locate_port(dir, type);
}

void genie::NodeReg::create_rvd()
{
    // Create either naked RVD ports, or use the RVD ports within TOPO ports

    RVDPort* inport;
    RVDPort* outport;

    if (m_notopo)
    {
        inport = (RVDPort*)add_port(new RVDPort(Dir::IN, INPORT_NAME));
        outport = (RVDPort*)add_port(new RVDPort(Dir::OUT, OUTPORT_NAME));
    }
    else
    {
        inport = get_input();
        outport = get_output();
    }

    inport->set_clock_port_name(CLOCKPORT_NAME);
    inport->add_role_binding(RVDPort::ROLE_VALID, new VlogStaticBinding("i_valid"));
    inport->add_role_binding(RVDPort::ROLE_READY, new VlogStaticBinding("o_ready"));
    inport->add_role_binding(RVDPort::ROLE_DATA_CARRIER, new VlogStaticBinding("i_data"));
    inport->get_bp_status().make_configurable();
    inport->get_proto().set_carried_protocol(&m_proto);
 
    outport->set_clock_port_name(CLOCKPORT_NAME);
    outport->add_role_binding(RVDPort::ROLE_VALID, new VlogStaticBinding("o_valid"));;
    outport->add_role_binding(RVDPort::ROLE_READY, new VlogStaticBinding("i_ready"));
    outport->add_role_binding(RVDPort::ROLE_DATA_CARRIER, new VlogStaticBinding("o_data"));
    outport->get_bp_status().make_configurable();
    outport->get_proto().set_carried_protocol(&m_proto);

    // Internally connect input to output and set its latency to 1, to allow graph
    // traversal and end-to-end latency calculation.
    auto link = (RVDLink*)connect(inport, outport, NET_RVD);
    link->set_latency(1);

    if (!m_notopo)
    {
        link->asp_get<ALinkContainment>()->add_parent_link(
            get_links(NET_TOPO).front());
    }
}

