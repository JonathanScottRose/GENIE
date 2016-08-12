#include "genie/node_clockx.h"
#include "genie/vlog_bind.h"
#include "genie/genie.h"

using namespace genie;
using namespace vlog;

namespace
{
	const std::string MODULE = "genie_clockx";

	const std::string INDATAPORT_NAME = "in_data";
	const std::string INCLOCKPORT_NAME = "in_clock";
	const std::string OUTDATAPORT_NAME = "out_data";
	const std::string OUTCLOCKPORT_NAME = "out_clock";
	const std::string RESETPORT_NAME = "reset";
}

NodeClockX::NodeClockX()
{
	auto vinfo = new NodeVlogInfo(MODULE);
	vinfo->add_port(new vlog::Port("arst", 1, vlog::Port::IN));
	vinfo->add_port(new vlog::Port("wrclk", 1, vlog::Port::IN));
	vinfo->add_port(new vlog::Port("rdclk", 1, vlog::Port::IN));
	vinfo->add_port(new vlog::Port("i_data", "WIDTH", vlog::Port::IN));
	vinfo->add_port(new vlog::Port("i_valid", 1, vlog::Port::IN));
	vinfo->add_port(new vlog::Port("o_ready", 1, vlog::Port::OUT));
	vinfo->add_port(new vlog::Port("o_data", "WIDTH", vlog::Port::OUT));
	vinfo->add_port(new vlog::Port("o_valid", 1, vlog::Port::OUT));
	vinfo->add_port(new vlog::Port("i_ready", 1, vlog::Port::IN));
	set_hdl_info(vinfo);

	auto clkport = add_port(new ClockPort(Dir::IN, INCLOCKPORT_NAME));
	clkport->add_role_binding(ClockPort::ROLE_CLOCK, new VlogStaticBinding("wrclk"));

	clkport = add_port(new ClockPort(Dir::IN, OUTCLOCKPORT_NAME));
	clkport->add_role_binding(ClockPort::ROLE_CLOCK, new VlogStaticBinding("rdclk"));

	auto rstport = add_port(new ResetPort(Dir::IN, RESETPORT_NAME));
	rstport->add_role_binding(ResetPort::ROLE_RESET, new VlogStaticBinding("arst"));

	auto inport = new RVDPort(Dir::IN, INDATAPORT_NAME);
	inport->set_clock_port_name(INCLOCKPORT_NAME);
	inport->add_role_binding(RVDPort::ROLE_VALID, new VlogStaticBinding("i_valid"));		
	inport->add_role_binding(RVDPort::ROLE_DATA_CARRIER, new VlogStaticBinding("i_data"));
    inport->add_role_binding(RVDPort::ROLE_READY, new VlogStaticBinding("o_ready"));
	inport->get_proto().set_carried_protocol(&m_proto);
    inport->get_bp_status().make_configurable();
	add_port(inport);

	auto outport = new RVDPort(Dir::OUT, OUTDATAPORT_NAME);
	outport->set_clock_port_name(OUTCLOCKPORT_NAME);
	outport->add_role_binding(RVDPort::ROLE_VALID, new VlogStaticBinding("o_valid"));
	outport->add_role_binding(RVDPort::ROLE_DATA_CARRIER, new VlogStaticBinding("o_data"));
    outport->add_role_binding(RVDPort::ROLE_READY, new VlogStaticBinding("i_ready"));
	outport->get_proto().set_carried_protocol(&m_proto);
    outport->get_bp_status().make_configurable();
	add_port(outport);

	connect(inport, outport, NET_RVD);
}

NodeClockX::~NodeClockX()
{
}

ClockPort* NodeClockX::get_inclock_port() const
{
	return (ClockPort*)get_port(INCLOCKPORT_NAME);
}

ClockPort* NodeClockX::get_outclock_port() const
{
	return (ClockPort*)get_port(OUTCLOCKPORT_NAME);
}

RVDPort* NodeClockX::get_indata_port() const
{
	return (RVDPort*)get_port(INDATAPORT_NAME);
}

RVDPort* NodeClockX::get_outdata_port() const
{
	return (RVDPort*)get_port(OUTDATAPORT_NAME);
}

HierObject* NodeClockX::instantiate() 
{
	throw HierException(this, "node not instantiable");
}

void NodeClockX::do_post_carriage()
{
	define_param("WIDTH", m_proto.get_total_width());
}

AreaMetrics NodeClockX::get_area_usage() const
{
    AreaMetrics result;

    auto& params = genie::arch_params();
    unsigned data_bits = m_proto.get_total_width();

    result.dist_ram = (data_bits + params.lutram_width-1) / params.lutram_width;
    result.regs = 53 + data_bits;

    return result;
}

