#include <unordered_set>
#include "genie/structure.h"
#include "genie/hierarchy.h"

using namespace genie;

//
// Port
//

Port::Port(Dir dir, NetType type)
: m_dir(dir), m_type(type)
{
}

/*
Port::Port(Dir dir, const std::string& name)
	: Port(dir)
{
	set_name(name);
}*/

Port::~Port()
{
	util::delete_all(m_role_bindings);
}

Port::Port(const Port& o)
	: HierObject(o), m_dir(o.m_dir), m_type(o.m_type)
{
	set_name(o.get_name());

	for (auto& b : o.m_role_bindings)
	{
		auto nb = new RoleBinding(*b);
		nb->set_parent(this);
		add_role_binding(nb);
	}
}

Node* Port::get_node() const
{
	HierObject* result = this->get_parent();

	while (result && !is_a<Node*>(result))
	{
		result = result->get_parent();
	} 

	return as_a<Node*>(result);
}

const SigRole& Port::get_role_def(SigRoleID id) const
{
	// Helper function, gets signal role definition from network definition of this port
	return Network::get(m_type)->get_sig_role(id);
}

RoleBinding* Port::add_role_binding(SigRoleID id, const std::string& tag, HDLBinding* bnd)
{
	return add_role_binding(new RoleBinding(id, tag, bnd));
}

RoleBinding* Port::add_role_binding(SigRoleID id, HDLBinding* bnd)
{
	return add_role_binding(new RoleBinding(id, bnd));
}

RoleBinding* Port::add_role_binding(const std::string& role, const std::string& tag, HDLBinding* bnd)
{
	auto id = Network::get(m_type)->role_id_from_name(role);
	return add_role_binding(new RoleBinding(id, tag, bnd));
}

RoleBinding* Port::add_role_binding(const std::string& role, HDLBinding* bnd)
{
	auto id = Network::get(m_type)->role_id_from_name(role);
	return add_role_binding(new RoleBinding(id, bnd));
}

RoleBinding* Port::add_role_binding(RoleBinding* b)
{
	auto role = get_role_def(b->get_id());

	if (role.get_uses_tags())
	{
		if (has_role_binding(b->get_id(), b->get_tag()))
		{
			throw HierException(this, "already has signal binding for role " +
				role.get_name() + " with tag " + b->get_tag());
		}
	}
	else
	{
		if (has_role_binding(b->get_id()))
		{
			throw HierException(this, "already has signal binding for role " +
				role.get_name());
		}
	}

	b->set_parent(this);

	m_role_bindings.push_back(b);
	return b;
}

std::vector<RoleBinding*> Port::get_role_bindings(SigRoleID id)
{
	std::vector<RoleBinding*> result;

	for (const auto& entry : m_role_bindings)
	{
		if (entry->get_id() == id)
			result.push_back(entry);
	}

	return result;
}

RoleBinding* Port::get_role_binding(SigRoleID id, const std::string& tag)
{
	// Used for roles that support tags
	assert(get_role_def(id).get_uses_tags());

	std::string ltag = util::str_tolower(tag);
	for (auto& entry : m_role_bindings)
	{
		if (entry->get_id() == id && entry->get_tag() == ltag)
			return entry;
	}

	return nullptr;
}

RoleBinding* Port::get_matching_role_binding(RoleBinding* other)
{
	if (get_role_def(other->get_id()).get_uses_tags())
	{
		return get_role_binding(other->get_id(), other->get_tag());
	}
	else
	{
		return get_role_binding(other->get_id());
	}
}

RoleBinding* Port::get_role_binding(SigRoleID id)
{
	// Used for roles that do not support tags
	assert(!get_role_def(id).get_uses_tags());

	for (auto& entry : m_role_bindings)
	{
		if (entry->get_id() == id)
			return entry;
	}

	return nullptr;
}

bool Port::has_role_binding(SigRoleID id)
{
	return get_role_binding(id) != nullptr;
}

bool Port::has_role_binding(SigRoleID id, const std::string& tag)
{
	return get_role_binding(id, tag) != nullptr;
}

bool Port::is_export() const
{
	return is_a<System*>(get_node());
}

HierObject* Port::instantiate()
{
	return new Port(*this);
}

//
// Node
//

Node::Node()
{
	init_resolv();
}

void Node::init_resolv()
{
	m_resolv = [this](const std::string& name)
	{
		auto param = get_param(name);

		if (param)
		{
			if (!param->is_bound())
				throw HierException(this, "parameter " + name + " is not bound");

			return param->get_expr();
		}
		else
		{
			auto sys = as_a<System*>(get_parent());

			if (sys) return sys->get_exp_resolver()(name);
			else throw Exception("undefined parameter " + name);
		}
	};
}

Node::Node(const Node& o)
	: HierObject(o)
{
	init_resolv();

	util::copy_all_2(o.m_params, m_params);
}

Node::Ports Node::get_ports(NetType net) const
{
	return get_children<Port>([=] (const HierObject* o)
	{
		const Port* p = dynamic_cast<const Port*>(o);
		return p && p->get_type() == net;
	});
}

Node::Ports Node::get_ports() const
{
	return get_children_by_type<Port>();
}

Port* Node::get_port(const std::string& name) const
{
	return as_a<Port*>(get_child(name));
}

Port* Node::add_port(Port* p)
{
	add_child(p);
	return p;
}

void Node::delete_port(const std::string& name)
{
	HierObject* p = remove_child(name);
	delete p;
}

HierObject* Node::instantiate()
{
	auto result = new Node(*this);

	for (auto child : m_children)
	{
		auto inst = child.second->instantiate();
		result->add_child(inst);
	}

	return result;
}


//
// System
//

System::System()
	: Node()
{
}

System::~System()
{
	// Clean up links
	for (auto& links : m_links)
	{
		// m_links is a map of Lists. here we delete each List
		util::delete_all(links.second);
	}
}

System::NetTypes System::get_net_types() const
{
	return util::keys<NetTypes>(m_links);
}

System::Links System::get_links() const
{
	Links result;
	for (const auto& i : m_links)
	{
		result.insert(result.end(), i.second.begin(), i.second.end());
	}
	return result;
}

System::Links System::get_links(NetType type) const
{
	auto it = m_links.find(type);
	return (it == m_links.end())? Links() : it->second;
}

System::Links System::get_links(HierObject* src, HierObject* sink, NetType nettype) const
{
	Links result;
	Endpoint* src_ep;
	Endpoint* sink_ep;

	get_eps(src, sink, nettype, src_ep, sink_ep);

	// Go to source endpoint, do linear search through all outgoing links 
	for (auto& link : src_ep->links())
	{
		if (link->get_sink_ep() == sink_ep)
		{
			result.push_back(link);
			break;
		}
	}

	return result;
}

System::Links System::get_links(HierObject* src, HierObject* sink) const
{
	NetType nettype = find_auto_net_type(src, sink);
	if (nettype == NET_INVALID)
		throw Exception("could not automatically determine network type");

	return get_links(src, sink, nettype);
}

bool System::verify_common_parent(HierObject* a, HierObject* b, bool& a_boundary,
	bool& b_boundary) const
{
	// Populate ancestries of a and b
	std::vector<HierObject*> a_line, b_line;

	while (a)
	{
		a_line.push_back(a);
		a = a->get_parent();
	}

	while (b)
	{
		b_line.push_back(b);
		b = b->get_parent();
	}

	// Find the last last parent (from the root) before lineages diverge.
	HierObject* common = nullptr;
	while (!a_line.empty() && !b_line.empty())
	{
		HierObject* cur_a = a_line.back();
		HierObject* cur_b = b_line.back();

		if (cur_a == cur_b)
		{
			// Ancestry still shared at this node? Advance result
			common = cur_a;
		}
		else
		{
			// Divergence. Last-updated result was the common parent.
			break;
		}

		// Pop shared ancestor from both lineages
		a_line.pop_back();
		b_line.pop_back();
	}

	// Whether the common ancestor is this System or not.
	bool result = (common == this);

	// The first remaining entry in a_line and b_line are the children of the common parent.
	// If either of them is a Port, then they are a top-level exported Port of this System, or one
	// of its decendants, and we return that fact.

	if (result)
	{
		a_boundary = !a_line.empty() && is_a<Port*>(a_line.back());
		b_boundary = !b_line.empty() && is_a<Port*>(b_line.back());
	}
	
	return result;
}

void System::get_eps(HierObject* src, HierObject* sink, NetType nettype,
	Endpoint*& src_ep, Endpoint*& sink_ep) const
{
	// Ensure src/sink are somewhere within this System
	bool src_boundary, sink_boundary;
	bool valid = verify_common_parent(src, sink, src_boundary, sink_boundary);

	if (!valid)
	{
		throw HierException(this, src->get_hier_path() + " and " + sink->get_hier_path() + 
		" are not children of this system");
	}

	// If src or sink is a (or is a child of) a System boundary port, then we need to use
	// its inward-facing Endpoint rather than its outward-facing one
	src_ep = src->get_endpoint(nettype, src_boundary? LinkFace::INNER : LinkFace::OUTER);
	sink_ep = sink->get_endpoint(nettype, sink_boundary? LinkFace::INNER : LinkFace::OUTER);

	if (!src_ep || !sink_ep)
	{
		std::string netname = Network::to_string(nettype);
		std::string who_role = !src_ep ? " source" : " sink";
		HierObject* who_obj = !src_ep ? src : sink;
		throw HierException(who_obj, "not a " + netname + who_role);
	}
}

Link* System::connect(HierObject* src, HierObject* sink, NetType net)
{
	Network* def = Network::get(net);
	Endpoint* src_ep;
	Endpoint* sink_ep;

	get_eps(src, sink, net, src_ep, sink_ep);

	// Create link and set its src/sink
	Link* link = def->create_link();
	link->set_src(src_ep);
	link->set_sink(sink_ep);

	// Hook up endpoints to the link
	src_ep->add_link(link);
	sink_ep->add_link(link);

	// Add the link to the system
	m_links[net].push_back(link);

	return link;
}

NetType System::find_auto_net_type(HierObject* src, HierObject* sink) const
{
	// Try to figure out the network type based on the endpoints
	NetType result = NET_INVALID;

	for (auto obj : { src, sink })
	{
		auto types = obj->get_connectable_networks();

		// Must have exactly one aspect that derives from Endpoint
		if (types.size() == 1)
		{
			result = types.front();
			break;
		}
	}

	return result;
}

Link* System::connect(HierObject* src, HierObject* sink)
{
	NetType nettype = find_auto_net_type(src, sink);

	if (nettype == NET_INVALID)
		throw Exception("Could not automatically determine network type to connect on");

	return connect(src, sink, nettype);
}

void System::disconnect(HierObject* src, HierObject* sink, NetType nettype)
{
	Endpoint* src_ep;
	Endpoint* sink_ep;

	get_eps(src, sink, nettype, src_ep, sink_ep);

	// Go through src's links, find the one whose sink is 'sink'
	for (auto link : src_ep->links())
	{
		if (link->get_sink_ep() == sink_ep)
		{
			// Found it. Disconnect and get out before we break the for loop
			disconnect(link);
			break;
		}
	}
}

void System::disconnect(Link* link)
{
	Endpoint* src_ep = link->get_src_ep();
	Endpoint* sink_ep = link->get_sink_ep();
	NetType nettype = link->get_type();

	// Disconnect the Link from its endpoints
	src_ep->remove_link(link);
	sink_ep->remove_link(link);

	// Remove the link from the System and destroy it
	util::erase(m_links[nettype], link);
	delete link;
}

void System::disconnect(HierObject* src, HierObject* sink)
{
	NetType nettype = find_auto_net_type(src, sink);

	if (nettype == NET_INVALID)
		throw Exception("could not automatically determine network type to disconnect on");

	disconnect(src, sink, nettype);
}

void System::splice(Link* orig, HierObject* new_sink, HierObject* new_src)
{
	//
	// orig_src --> orig --> orig_sink
	//
	// becomes
	//
	// orig_src --> orig --> new_sink, new_src --> (new link) --> orig_sink
	//

	// Check containment: we cannot splice if this link has child links
	ALinkContainment* acont = orig->asp_get<ALinkContainment>();
	if (acont && acont->has_child_links())
		throw Exception("can not splice link: has child links");

	Endpoint* orig_src_ep = orig->get_src_ep();
	Endpoint* orig_sink_ep = orig->get_sink_ep();
	NetType net = orig->get_type();

	Endpoint* new_sink_ep;
	Endpoint* new_src_ep;
	get_eps(new_sink, new_src, net, new_sink_ep, new_src_ep);

	// Disconnect old link from its sink and attach it to new sink
	orig->set_sink(new_sink_ep);
	orig_sink_ep->remove_link(orig);
	new_sink_ep->add_link(orig);

	// Create a link from new src to old sink
	Link* new_link = Network::get(net)->create_link();
	new_link->set_src(new_src_ep);
	new_link->set_sink(new_sink_ep);
	new_src_ep->add_link(new_link);
	new_sink_ep->add_link(new_link);

	// Copy parent containment
	if (acont)
	{
		auto newcont = new_link->asp_add(new ALinkContainment());
		auto parents = acont->get_parent_links();
		for (auto parent : parents)
		{
			newcont->add_parent_link(parent);
		}
	}
}

System::Objects System::get_objects() const
{
	return get_children();
}

System::Nodes System::get_nodes() const
{
	return get_children_by_type<Node>();
}

System::Exports System::get_exports() const
{
	return get_children_by_type<Port>();
}

void System::add_object(HierObject* obj)
{
	// System's Parent aspect
	add_child(obj);
}

HierObject* System::get_object(const HierPath& path) const
{
	return get_child(path);
}

void System::delete_object(const HierPath& path)
{
	HierObject* obj = remove_child(path);
	delete obj;
}

namespace
{
	void write_dot_r(
		HierObject* cur_obj,
		std::ofstream& out,
		const std::unordered_set<HierObject*>& live_eps,
		const std::unordered_set<HierObject*>& live_parents)
	{
		// Define subgraph, give label
		out << "subgraph cluster_" << cur_obj->get_name() << " {" << std::endl;
		out << "label=\"" << cur_obj->get_name() << "\"" << std::endl;
		
		// This object is guaranteed to be a parent. Get children.
		auto children = cur_obj->get_children();

		for (auto child : children)
		{
			// Each child is either a terminal endpoint (src/sink of a link), or
			// a parent of one. If neither, then ignore it.
			if (live_eps.count(child))
			{
				// Src/sink of a link: give it a proper label
				out << "\"" << child->get_hier_path() << "\" [label=\""
					<< child->get_name() << "\"];" << std::endl;
			}
			else if (live_parents.count(child))
			{
				// Parent of an endpoint: recurse to create a new subgraph
				write_dot_r(child, out, live_eps, live_parents);
			}
		}

		// Close subgraph before going up a level
		out << "}" << std::endl;
	}
}

void System::write_dot(const std::string& filename, NetType nettype)
{
	// Gather connected objects into a set.
	// Also keep track of all parent objects of live objects.
	std::unordered_set<HierObject*> live_eps;
	std::unordered_set<HierObject*> live_parents;
	
	auto links = get_links(nettype);
	for (auto& link: links)
	{
		for (auto obj : { link->get_src(), link->get_sink() })
		{
			// all containers of source/sink are live
			for (auto parent = obj->get_parent(); parent != this; parent = parent->get_parent())
				live_parents.insert(parent);

			// source and sink are live
			live_eps.insert(obj);			
		}
	}

	// Open file, create top-level graph
	std::ofstream out(filename + ".dot");
	out << "digraph {" << std::endl;
	out << "newrank=true;" << std::endl;
	//out << "ranksep=true;" << std::endl;

	// Do recursive subgraph creation for parents of live endpoints
	auto objects = this->get_objects();
	for (auto obj : objects)
	{
		if (live_parents.count(obj))
			write_dot_r(obj, out, live_eps, live_parents);
	}

	// For top-level system graph: create edge for every link
	for (auto& link : links)
	{
		HierObject* src = link->get_src();
		HierObject* sink = link->get_sink();

		out << "\"" << src->get_hier_path() 
			<< "\" -> \"" << sink->get_hier_path()
			<< "\";" 
			<< std::endl;
	}

	// Finish main graph
	out << "}" << std::endl;
	out.close();
}

//
// HierRoot
//

HierRoot::HierRoot()
{
}

HierRoot::~HierRoot()
{
}

const std::string& HierRoot::get_name() const
{
	static std::string NAME(genie::make_reserved_name("root"));
	return NAME;
}


List<System*> HierRoot::get_systems()
{
	return get_children_by_type<System>();
}

List<Node*> HierRoot::get_non_systems()
{
	return get_children<Node>([](const HierObject* o)
	{
		return as_a<const System*>(o) == nullptr;
	});
}
