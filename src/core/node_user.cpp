#include "pch.h"
#include "node_user.h"
#include "port_rs.h"
#include "net_rs.h"

using namespace genie::impl;

genie::Link * NodeUser::create_internal_link(genie::PortRS * src, genie::PortRS * sink,
	unsigned latency)
{
	auto src_imp = dynamic_cast<PortRS*>(src);
	auto sink_imp = dynamic_cast<PortRS*>(sink);

	for (auto obj : { src_imp, sink_imp })
	{
		if (!obj)
		{
			throw Exception(get_hier_path() + ": can't create internal link because " +
				obj->get_hier_path() + " is not an RS Port");
		}
	}

	auto link = static_cast<LinkRSPhys*>(this->connect(src_imp, sink_imp, NET_RS_PHYS));
	link->set_latency(latency);
	return link;
}

NodeUser::NodeUser(const std::string & name, const std::string & hdl_name)
    : Node(name, hdl_name)
{
}

NodeUser* NodeUser::instantiate() const
{
	return clone();
}

void NodeUser::prepare_for_hdl()
{
}

void NodeUser::annotate_timing()
{
	// do nothing for user nodes
}

AreaMetrics NodeUser::annotate_area()
{
	return AreaMetrics();
}

NodeUser* NodeUser::clone() const
{
	auto result = new NodeUser(*this);

	// Now copy links and link relations
	result->copy_links_from(*this);
	result->m_link_rel = m_link_rel;

	return result;
}

NodeUser::NodeUser(const NodeUser& o)
    : Node(o)
{
}
