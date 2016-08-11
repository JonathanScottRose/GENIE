#include "genie/node_mdelay.h"
#include "genie/net_clock.h"
#include "genie/net_reset.h"
#include "genie/net_rvd.h"
#include "genie/vlog_bind.h"
#include "genie/node_mdelay.h"
#include "genie/genie.h"

using namespace genie;
using namespace vlog;

namespace
{
    const std::string MODNAME = "genie_mem_delay";
    const std::string INPORT_NAME = "in";
    const std::string OUTPORT_NAME = "out";
    const std::string CLOCKPORT_NAME = "clk";
    const std::string RESETPORT_NAME = "reset";
}

NodeMDelay::NodeMDelay()
{
    // Create verilog ports
    auto vinfo = new NodeVlogInfo(MODNAME);
    vinfo->add_port(new vlog::Port("clk", 1, vlog::Port::IN));
    vinfo->add_port(new vlog::Port("reset", 1, vlog::Port::IN));
    vinfo->add_port(new vlog::Port("i_data", "WIDTH", vlog::Port::IN));
    vinfo->add_port(new vlog::Port("i_valid", 1, vlog::Port::IN));
    vinfo->add_port(new vlog::Port("o_ready", 1, vlog::Port::OUT));
    vinfo->add_port(new vlog::Port("o_data", "WIDTH", vlog::Port::OUT));
    vinfo->add_port(new vlog::Port("o_valid", 1, vlog::Port::OUT));
    vinfo->add_port(new vlog::Port("i_ready", 1, vlog::Port::IN));
    set_hdl_info(vinfo);

    // Create genie ports and port bindings
    auto clkport = add_port(new ClockPort(Dir::IN, CLOCKPORT_NAME));
    clkport->add_role_binding(ClockPort::ROLE_CLOCK, new VlogStaticBinding("clk"));

    auto resetport = add_port(new ResetPort(Dir::IN, RESETPORT_NAME));
    resetport->add_role_binding(ResetPort::ROLE_RESET, new VlogStaticBinding("reset"));

    auto inport = new RVDPort(Dir::IN, INPORT_NAME);
    inport->set_clock_port_name(CLOCKPORT_NAME);
    inport->add_role_binding(RVDPort::ROLE_VALID, new VlogStaticBinding("i_valid"));
    inport->add_role_binding(RVDPort::ROLE_READY, new VlogStaticBinding("o_ready"));
    inport->add_role_binding(RVDPort::ROLE_DATA_CARRIER, new VlogStaticBinding("i_data"));
    inport->get_bp_status().make_configurable();
    inport->get_proto().set_carried_protocol(&m_proto);
    add_port(inport);

    auto outport = new RVDPort(Dir::OUT, OUTPORT_NAME);
    outport->set_clock_port_name(CLOCKPORT_NAME);
    outport->add_role_binding(RVDPort::ROLE_VALID, new VlogStaticBinding("o_valid"));
    outport->add_role_binding(RVDPort::ROLE_READY, new VlogStaticBinding("i_ready"));
    outport->add_role_binding(RVDPort::ROLE_DATA_CARRIER, new VlogStaticBinding("o_data"));
    outport->get_bp_status().make_configurable();
    outport->get_proto().set_carried_protocol(&m_proto);
    add_port(outport);

    m_int_link = (RVDLink*)connect(inport, outport, NET_RVD);
}

RVDPort* NodeMDelay::get_input() const
{
    return as_a<RVDPort*>(get_port(INPORT_NAME));
}

RVDPort* NodeMDelay::get_output() const
{
    return as_a<RVDPort*>(get_port(OUTPORT_NAME));
}

void genie::NodeMDelay::set_delay(unsigned delay)
{
    // We should not be using this for just 1 cycle
    assert(delay > 1);

    // Change the internal latency on the RVD link
    m_int_link->set_latency(delay);
    
    // Update member var
    m_delay = delay;

    // Create/update verilog parameter
    define_param("CYCLES", delay);
}

HierObject* NodeMDelay::instantiate()
{
    throw HierException(this, "node not instantiable");
}

void NodeMDelay::do_post_carriage()
{
    define_param("WIDTH", m_proto.get_total_width());
}

AreaMetrics NodeMDelay::get_area_usage() const
{
    AreaMetrics result;

    const unsigned lutram_width =  genie::arch_params().lutram_width;
    const unsigned lutram_depth = genie::arch_params().lutram_depth;

    unsigned dwidth = m_proto.get_total_width();
    unsigned banks = (dwidth + lutram_width-1) / lutram_width;
    unsigned depth = (m_delay + lutram_depth-1) / lutram_depth;

    result.dist_ram = banks * depth;

    return result;
}

