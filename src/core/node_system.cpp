#include "pch.h"
#include "hierarchy.h"
#include "node_system.h"
#include "net_clockreset.h"
#include "net_conduit.h"
#include "net_rs.h"
#include "net_topo.h"
#include "port.h"
#include "node_split.h"
#include "node_merge.h"
#include "node_user.h"
#include "flow.h"
#include "genie_priv.h"

using namespace genie::impl;
using Dir = genie::Port::Dir;

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

genie::LinkRS * NodeSystem::create_rs_link(genie::HierObject * src, genie::HierObject * sink,
	unsigned src_addr, unsigned sink_addr)
{
	auto src_imp = dynamic_cast<HierObject*>(src);
	auto sink_imp = dynamic_cast<HierObject*>(sink);

	auto link = connect(src_imp, sink_imp, NET_RS_LOGICAL);
	auto link_rs = static_cast<LinkRSLogical*>(link);

	link_rs->set_src_addr(src_addr);
	link_rs->set_sink_addr(sink_addr);

	return link_rs;
}

genie::LinkTopo * NodeSystem::create_topo_link(genie::HierObject * src, genie::HierObject * sink)
{
	auto src_imp = dynamic_cast<HierObject*>(src);
	auto sink_imp = dynamic_cast<HierObject*>(sink);

	auto link = connect(src_imp, sink_imp, NET_TOPO);
	return static_cast<LinkTopo*>(link);
}

genie::Node * NodeSystem::create_instance(const std::string & mod_name,
	const std::string & inst_name)
{
	// Check if module exists (child of root with this name exists)
	auto prototype = genie::impl::get_node(mod_name);
	if (!prototype)
	{
		throw genie::Exception("unknown module: " + mod_name);
	}

	// Check if this system already has a child named inst_name
	if (has_child(inst_name))
	{
		throw genie::Exception(get_hier_path() + ": already has instance named " + inst_name);
	}

	auto iinst = dynamic_cast<IInstantiable*>(prototype);
	if (!iinst)
	{
		throw genie::Exception("module not instantiable: " + mod_name);
	}

	auto instance = iinst->instantiate();
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
	auto exlink_sink = new_port;
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

void NodeSystem::make_exclusive(const std::vector<genie::LinkRS*>& links)
{
	auto& excl = get_link_exclusivity();

	for (auto it1 = links.begin(); it1 != links.end(); ++it1)
	{
		auto impl1 = dynamic_cast<LinkRSLogical*>(*it1);
		assert(impl1);

		for (auto it2 = it1 + 1; it2 != links.end(); ++it2)
		{
			auto impl2 = dynamic_cast<LinkRSLogical*>(*it2);
			assert(impl2);

			excl.add(impl1->get_id(), impl2->get_id());
		}
	}
}

void NodeSystem::add_sync_constraint(const genie::SyncConstraint & constraint)
{
	//TODO: verify
	auto& cnst = get_sync_constraints();
	cnst.push_back(constraint);
}

void NodeSystem::set_max_logic_depth(unsigned max_depth)
{
	m_max_logic_depth = max_depth;
}

//
// Internal
//

NodeSystem::NodeSystem(const std::string & name)
    : Node(name, name), 
	m_excl_info(std::make_shared<ExclusivityInfo>()),
	m_sync_constraints(std::make_shared<SyncConstraints>())
{
	// Max logic depth defaults to global setting
	m_max_logic_depth = genie::impl::get_flow_options().max_logic_depth;
}

NodeSystem::~NodeSystem()
{
}

Node* NodeSystem::instantiate() const
{
	// Instantiating a System yields a user node
	auto result = new NodeUser(get_name(), get_hdl_name());

	// Copy HDL state
	result->set_hdl_state(const_cast<hdl::HDLState&>(m_hdl_state));

	// Copy ports
	for (auto port : get_children_by_type<Port>())
	{
		result->add_child(port->clone());
	}

	return result;
}

void NodeSystem::prepare_for_hdl()
{
}

void NodeSystem::annotate_timing()
{
	// this should never be called
	assert(false);
}

AreaMetrics NodeSystem::annotate_area()
{
	return AreaMetrics();
}

NodeSystem* NodeSystem::clone() const
{
	auto result = new NodeSystem(*this);
	
	// Copy child nodes
	for (auto child : get_children_by_type<Node>())
	{
		auto newc = child->clone();
		result->add_child(newc);
	}

	// Now copy links and link relations
	result->copy_links_from(*this);
	result->m_link_rel = m_link_rel;
	result->m_link_rel.prune(result);

	return result;
}

std::vector<Node*> NodeSystem::get_nodes() const
{
    return get_children_by_type<Node>();
}

ExclusivityInfo & NodeSystem::get_link_exclusivity() const
{
	return *m_excl_info.get();
}

SyncConstraints & NodeSystem::get_sync_constraints() const
{
	return *m_sync_constraints.get();
}

NodeSystem * NodeSystem::create_snapshot(
	const std::unordered_set<HierObject*>& preserve_objs, 
	const std::vector<Link*>& preserve_links)
{
	// Copies the node and ports only
	auto result = new NodeSystem(*this);

	// Make clones of those nodes into the snapshot
	for (auto node : preserve_objs)
	{
		result->add_child(node->clone());
	}

	// Copy links
	result->copy_links_from(*this, &preserve_links);

	// Copy link relations and prune it
	result->m_link_rel = m_link_rel;
	result->m_link_rel.prune(result);

	return result;
}

void NodeSystem::reintegrate_snapshot(NodeSystem * src)
{
	// Find links in src that don't exist here.
	// Record these links in a map that remembers their src/sink by string name.
	// Disconnect the links from their src/sink in 'src' system, leaving no pointers to them
	//
	// Then, move all new Nodes from src system to this system. Because of link disconnection
	// above, they do not refer to any Links yet.
	// For any nodes that DO exist in both src and here, do special updating for those.
	//
	// Take all the links we remembered before, move them into this system, and reconnect
	// them using string src/sink names.
	
	struct MovedLink
	{
		Link* link;
		std::string src;
		std::string sink;
	};

	std::vector<MovedLink> moved_links;

	// Move new links from src system here, one network type at a time
	for (auto& src_cont : src->m_links)
	{
		auto& this_cont = this->get_links_cont(src_cont.get_type());
		auto new_links_of_type = this_cont.move_new_from(src_cont);
		
		for (auto link : new_links_of_type)
		{
			// Although the link has been moved here, it still connects to objects
			// in the source system. Remember the old endpoints and disconnect it.
			MovedLink ml;
			ml.link = link;
			ml.src = link->get_src()->get_hier_path(src);
			ml.sink = link->get_sink()->get_hier_path(src);

			moved_links.push_back(ml);

			link->get_src_ep()->remove_link(link);
			link->get_sink_ep()->remove_link(link);
			link->set_src_ep(nullptr);
			link->set_sink_ep(nullptr);
		}
	}

	// 
	// New links have been plucked out.
	// Now, handle the child objects (Nodes and Ports).
	// Identify the ones in src that either:
	// 1) exist in 'this'
	// 2) do not exist in 'this'
	//
	// The latter are simply moved, changing ownership and parent/child relationships.
	// For the former, the existing version here stays in place, and a type-specific
	// reintegration handler is called to copy over state changes

	for (auto child : src->get_children())
	{
		auto child_path = child->get_hier_path(src);
		auto exist_child = dynamic_cast<HierObject*>(this->get_child(child_path));

		if (exist_child)
		{
			// Update existing one with new state
			exist_child->reintegrate(child);
		}
		else
		{
			// Move ownership to this system
			src->remove_child(child);
			this->add_child(child);
		}
	}

	// Now, with all new nodes moved, reconnected the moved links
	for (auto& ml : moved_links)
	{
		auto link = ml.link;

		// Get endpoints by name in this system
		auto src_obj = dynamic_cast<HierObject*>(get_child(ml.src));
		auto sink_obj = dynamic_cast<HierObject*>(get_child(ml.sink));
		assert(src_obj && sink_obj);

		auto src_ep = src_obj->get_endpoint(link->get_type(), Dir::OUT);
		auto sink_ep = sink_obj->get_endpoint(link->get_type(), Dir::IN);
		assert(src_ep && sink_ep); // TODO: can new endpoints be created?

		// Do the connection
		link->set_src_ep(src_ep);
		link->set_sink_ep(sink_ep);
		src_ep->add_link(link);
		sink_ep->add_link(link);
	}

	// Reintegrate link relations
	m_link_rel.reintegrate(src->m_link_rel);
}

NodeSystem::NodeSystem(const NodeSystem& o)
    : Node(o), 
	m_excl_info(o.m_excl_info),
	m_sync_constraints(o.m_sync_constraints),
	m_max_logic_depth(o.m_max_logic_depth)
{
}

//
// Exclusivity Info
//

void ExclusivityInfo::add(LinkID link1, LinkID link2)
{
	// Make canonical ordering
	if (link2 < link1)
		std::swap(link1, link2);

	m_sets[link1].insert(link2);
}

ExclusivityInfo::Set & ExclusivityInfo::get_set(LinkID id)
{
	return m_sets[id];
}

bool ExclusivityInfo::are_exclusive(LinkID link1, LinkID link2)
{
	if (link2 < link1)
		std::swap(link1, link2);

	return m_sets[link1].count(link2) > 0;
}
