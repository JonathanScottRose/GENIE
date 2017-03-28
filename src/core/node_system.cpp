#include "pch.h"
#include "node_system.h"
#include "net_clockreset.h"
#include "net_conduit.h"
#include "net_rs.h"
#include "net_topo.h"
#include "port.h"
#include "node_split.h"
#include "node_merge.h"
#include "genie_priv.h"

using namespace genie::impl;

//
// Public
//

void NodeSystem::create_sys_param(const std::string & name)
{
    set_param(name, new NodeSysParam());
}

genie::Link * NodeSystem::create_clock_link(genie::HierObject * src, genie::HierObject * sink)
{
	auto src_imp = dynamic_cast<HierObject*>(src);
	auto sink_imp = dynamic_cast<HierObject*>(sink);

	return connect(src_imp, sink_imp, NET_CLOCK);
}

genie::Link * NodeSystem::create_reset_link(genie::HierObject * src, genie::HierObject * sink)
{
	auto src_imp = dynamic_cast<HierObject*>(src);
	auto sink_imp = dynamic_cast<HierObject*>(sink);

	return connect(src_imp, sink_imp, NET_RESET);
}

genie::Link * NodeSystem::create_conduit_link(genie::HierObject * src, genie::HierObject * sink)
{
	auto src_imp = dynamic_cast<HierObject*>(src);
	auto sink_imp = dynamic_cast<HierObject*>(sink);

	return connect(src_imp, sink_imp, NET_CONDUIT);
}

genie::LinkRS * NodeSystem::create_rs_link(genie::HierObject * src, genie::HierObject * sink)
{
	auto src_imp = dynamic_cast<HierObject*>(src);
	auto sink_imp = dynamic_cast<HierObject*>(sink);

	auto link = connect(src_imp, sink_imp, NET_RS_LOGICAL);
	return dynamic_cast<genie::LinkRS*>(link);
}

genie::LinkTopo * NodeSystem::create_topo_link(genie::HierObject * src, genie::HierObject * sink)
{
	auto src_imp = dynamic_cast<HierObject*>(src);
	auto sink_imp = dynamic_cast<HierObject*>(sink);

	auto link = connect(src_imp, sink_imp, NET_TOPO);
	return dynamic_cast<genie::LinkTopo*>(link);
}

genie::Node * NodeSystem::create_instance(const std::string & mod_name, 
	const std::string & inst_name)
{
	// Check if module exists (child of root with this name exists)
	auto prototype = dynamic_cast<Node*>(genie::impl::get_node(mod_name));
	if (!prototype)
	{
		throw genie::Exception("unknown module: " + mod_name);
	}

	// Check if this system already has a child named inst_name
	if (has_child(inst_name))
	{
		throw genie::Exception(get_hier_path() + ": already has instance named " + inst_name);
	}

	auto instance = prototype->instantiate();
	instance->set_name(inst_name);
	add_child(instance);

	return instance;
}

genie::Port * NodeSystem::export_port(genie::Port * orig, const std::string & opt_name)
{
	auto orig_impl = dynamic_cast<Port*>(orig);
	assert(orig);

	// We must resolve parameters first
	resolve_params();
	orig_impl->get_node()->resolve_params();

	// 1) Check if port is exportable: must not already belong to the system
	if (orig_impl->get_node() == this)
	{
		throw Exception(get_hier_path() + ": port " + orig->get_hier_path(this) +
			" is already a top-level port of the system");
	}

	// 2) Make a new name for the exported port, automatically if none specified
	std::string new_name;
	if (opt_name.empty())
	{
		new_name = orig->get_hier_path(this);
		std::replace(new_name.begin(), new_name.end(), HierObject::PATH_SEP, '_');
	}
	else
	{
		new_name = opt_name;
	}

	// The remainder of the work is port-type specific
	auto new_port = orig_impl->export_port(new_name, this);

	// Now we just make a connection from the old to the new.
	// The exported port, which belongs to a Node within this system, can be looked
	// at to dictate the connection direction. If it's an IN, it will be the sink, and if
	// it's an OUT, it will be the source.
	
	auto ptinfo = genie::impl::get_port_type(orig_impl->get_type());

	auto exlink_src = orig_impl; // assume original port is the source (OUT)
	auto exlink_sink = dynamic_cast<Port*>(new_port);
	if (orig_impl->get_dir() == Port::Dir::IN)
		std::swap(exlink_src, exlink_sink);

	connect(exlink_src, exlink_sink, ptinfo->get_default_network());

	return new_port;
}

genie::Node * NodeSystem::create_split(const std::string & opt_name)
{
	std::string name;
	name = opt_name.empty() ? make_unique_child_name("sp") : opt_name;

	auto node = new NodeSplit();
	node->set_name(name);

	add_child(node);
	
	return node;
}

genie::Node * NodeSystem::create_merge(const std::string & opt_name)
{
	std::string name;
	name = opt_name.empty() ? make_unique_child_name("mg") : opt_name;

	auto node = new NodeMerge();
	node->set_name(name);

	add_child(node);

	return node;
}

//
// Internal
//

NodeSystem::NodeSystem(const std::string & name)
    : Node(name, name)
{
}

Node* NodeSystem::instantiate()
{
    return new NodeSystem(*this);
}

std::vector<Node*> NodeSystem::get_nodes() const
{
    return get_children_by_type<Node>();
}

NodeSystem::NodeSystem(const NodeSystem& o)
    : Node(o)
{
}
