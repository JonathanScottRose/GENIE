#include "pch.h"
#include "node_user.h"
#include "port_rs.h"
#include "net_internal.h"

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

	return this->connect(src_imp, sink_imp, NET_INTERNAL);
}

NodeUser::NodeUser(const std::string & name, const std::string & hdl_name)
    : Node(name, hdl_name)
{
}

NodeUser* NodeUser::instantiate() const
{
    return new NodeUser(*this);
}

NodeUser* NodeUser::clone() const
{
	return new NodeUser(*this);
}

NodeUser::NodeUser(const NodeUser& o)
    : Node(o)
{
}
