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
	set_connectable(type, dir);
}

Port::~Port()
{
	util::delete_all(m_role_bindings);
	for (auto& i : m_endpoints)
	{
		delete i.second.first;
		delete i.second.second;
	}
}

Endpoint* Port::get_ep_by_face(const EndpointsEntry& p, LinkFace f)
{
	switch (f)
	{
	case LinkFace::OUTER: return p.first; break;
	case LinkFace::INNER: return p.second; break;
	default: assert(false);
	}

	return nullptr;
}

void Port::set_ep_by_face(EndpointsEntry& p, LinkFace f, Endpoint* ep)
{
	switch (f)
	{
	case LinkFace::OUTER: p.first = ep; break;
	case LinkFace::INNER: p.second = ep; break;
	default: assert(false);
	}
}

Port::NetTypes Port::get_connectable_networks() const
{
	return util::keys<NetTypes, EndpointsMap>(m_endpoints);
}

bool Port::is_connectable(NetType type) const
{
	return m_endpoints.count(type) > 0;
}

bool Port::is_connected(NetType type) const
{
	return is_connectable(type) &&
	(
		m_endpoints.at(type).first->is_connected() ||
		m_endpoints.at(type).second->is_connected()
	);
}

Endpoint* Port::get_endpoint_sysface(NetType type) const
{
	auto face = is_export()? LinkFace::INNER : LinkFace::OUTER;
	return get_endpoint(type, face);
}

Endpoint* Port::get_endpoint(NetType type, LinkFace face) const
{
	Endpoint* result = nullptr;

	auto it = m_endpoints.find(type);
	if (it != m_endpoints.end())
	{
		return get_ep_by_face(it->second, face);
	}

	return result;
}

void Port::set_connectable(NetType type, Dir dir)
{
	Network* ndef = Network::get(type);
	assert(type != NET_INVALID);

	if (is_connectable(type))
		throw HierException(this, "already connectable for nettype " + ndef->get_name());

	Endpoint* outer = new Endpoint(type, dir);
	Endpoint* inner = new Endpoint(type, dir_rev(dir));

	outer->set_obj(this);
	inner->set_obj(this);

	// Set default # of connection limits
	int max_sink = ndef->get_default_max_in();
	int max_src = ndef->get_default_max_out();
	Endpoint* sink = outer;
	Endpoint* src = inner;
	if (dir == Dir::OUT)
		std::swap(sink, src);

	src->set_max_links(max_src);
	sink->set_max_links(max_sink);
	
	// Put it in
	EndpointsEntry entry;
	set_ep_by_face(entry, LinkFace::OUTER, outer);
	set_ep_by_face(entry, LinkFace::INNER, inner);

	m_endpoints.emplace(type, entry);
}

void Port::set_unconnectable(NetType type)
{
	m_endpoints.erase(type);
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
	auto role = SigRole::get(b->get_id());

	if (role->get_uses_tags())
	{
		if (!b->has_tag())
		{
			throw HierException(this, "signal binding for role " + role->get_name() + " requires a tag");
		}

		if (has_role_binding(b->get_id(), b->get_tag()))
		{
			throw HierException(this, "already has signal binding for role " +
				role->get_name() + " with tag " + b->get_tag());
		}
	}
	else
	{
		if (b->has_tag())
		{
			throw HierException(this, "signal binding for role " + role->get_name() + " does not use tags");
		}

		if (has_role_binding(b->get_id()))
		{
			throw HierException(this, "already has signal binding for role " +
				role->get_name());
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
	assert(SigRole::get(id)->get_uses_tags());

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
	if (SigRole::get(other->get_id())->get_uses_tags())
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
	assert(!SigRole::get(id)->get_uses_tags());

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

void Port::clear_role_bindings()
{
	util::delete_all(m_role_bindings);
}

bool Port::is_export() const
{
	return is_a<System*>(get_node());
}

Port* Port::get_primary_port() const
{
	Port* result = const_cast<Port*>(this);
	while (Port* parent = as_a<Port*>(result->get_parent()))
	{
		result = parent;		
	}
	return result;
}

Port::Port(const Port& o)
	: HierObject(o), m_type(o.m_type), m_dir(o.m_dir)
{
	// Instantiated ports have the same name, dir, type as the thing they are instantiating
	set_name(o.get_name());

	// Copy connectivity
	for (const auto& i : o.m_endpoints)
	{
		auto new_inner = new Endpoint(*i.second.first);
		auto new_outer = new Endpoint(*i.second.second);

		new_inner->set_obj(this);
		new_outer->set_obj(this);

		auto type = i.first;
		m_endpoints.emplace(std::make_pair(type, EndpointsEntry(new_inner, new_outer)));
	}

	// Copy the role bindings
	for (auto& b : o.m_role_bindings)
		add_role_binding(new RoleBinding(*b));
}

void Port::set_max_links(NetType type, Dir dir, int max_links)
{
	// Get endpoint
	if (!is_connectable(type))
		throw HierException(this, " not connectable on type " + Network::to_string(type));

	auto& entry = m_endpoints[type];
	Endpoint* ep = entry.first->get_dir() == dir ? entry.first : entry.second;
	assert(ep->get_dir() == dir);

	ep->set_max_links(max_links);
}

Port* Port::locate_port(Dir dir, NetType type)
{
	if (type == NET_INVALID || is_connectable(type))
		return this;
	else
		return HierObject::locate_port(dir, type);
}

//
// Node
//

Node::Node()
	: m_hdl_info(nullptr)
{
}

Node::Node(const Node& o)
	: HierObject(o), m_hdl_info(nullptr)
{
	// Copy parameters
	util::copy_all_2(o.m_params, m_params);
	
	// Copy HDL info
	set_hdl_info(o.m_hdl_info->instantiate());

	// Instantiate all ports
	auto ports = o.get_ports();
	for (auto& p : ports)
	{
		add_child(p->instantiate());
	}  
}

Node::~Node()
{
	delete m_hdl_info;

	// Clean up links
	for (auto& links : m_links)
	{
		// m_links is a map of Lists. here we delete each List
		util::delete_all(links.second);
	}
}

expressions::NameResolver Node::get_exp_resolver()
{
	return [this](const std::string& name)
	{
		auto param = get_param(name);
		auto sys = as_a<System*>(get_parent());
	
		// Does a parameter definition exist for this node?
		if (param)
		{
			// Must have a value
			if (!param->is_bound())
				throw HierException(this, "parameter " + name + " is not bound");

			// If this Node is not a System: the parameter's value may be an expression that
			// references other parameters defined within the Node's parent System.
			// If this Node is a System: the parameter's value must evaluate to a constant.
			const auto& second_resolv = sys? sys->get_exp_resolver() : 
				Expression::get_const_resolver();

			return param->get_expr().get_value(second_resolv);
		}
		else
		{
			// Non-existent parameter? Well if we have a parent System, try looking it up there.
			if (sys) return sys->get_exp_resolver()(name);
			else throw HierException(this, "can't resolve parameter " + name);
		}
	};
}

List<ParamBinding*> Node::get_params(bool are_bound)
{
	List<ParamBinding*> result;
	for (auto& i : m_params)
	{
		auto param = i.second;
		if (param->is_bound() == are_bound)
			result.push_back(param);
	}

	return result;
}

ParamBinding* Node::define_param(const std::string& nm)
{
	return add_param(new ParamBinding(nm));
}

ParamBinding* Node::define_param(const std::string& nm, const Expression& exp)
{
	// If existing param binding, update its bound expression.
	// Otherwise, create a new param binding with the expression already bound.
	ParamBinding* result = get_param(nm);
	if (result)
		result->set_expr(exp);
	else
		result = add_param(new ParamBinding(nm, exp));

	return result;
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
	return new Node(*this);
}

void Node::set_hdl_info(NodeHDLInfo* info)
{
	delete m_hdl_info;
	m_hdl_info = info;
	info->set_node(this);
}

Node::NetTypes Node::get_net_types() const
{
	return util::keys<NetTypes>(m_links);
}

Node::Links Node::get_links() const
{
	Links result;
	for (const auto& i : m_links)
	{
		result.insert(result.end(), i.second.begin(), i.second.end());
	}
	return result;
}

Node::Links Node::get_links(NetType type) const
{
	auto it = m_links.find(type);
	return (it == m_links.end())? Links() : it->second;
}

Node::Links Node::get_links(HierObject* src, HierObject* sink, NetType nettype) const
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

Node::Links Node::get_links(HierObject* src, HierObject* sink) const
{
	NetType nettype = find_auto_net_type(src, sink);
	if (nettype == NET_INVALID)
		throw Exception("could not automatically determine network type");

	return get_links(src, sink, nettype);
}

void Node::get_eps(HierObject*& src, HierObject*& sink, NetType nettype,
	Endpoint*& src_ep, Endpoint*& sink_ep) const
{
	// Find the ports to connect.
	Port* src_port = src->locate_port(Dir::OUT, nettype);
	Port* sink_port = sink->locate_port(Dir::IN, nettype);

	// Find out where each Port lies within this Node:
	// 1) Port is at the boundary of this Node: use inward-facing endpoint to connect
	// 2) Port is at the boundary of a Node whose parent is this Node: use outward-facing endpoint
	// 3) Anything else: error
	LinkFace src_face, sink_face;
	for (auto& i : { std::make_pair(&src_face, src_port), std::make_pair(&sink_face, sink_port) })
	{
		LinkFace* pface = i.first;
		Port* port = i.second;
		Node* portnode = port->get_node();
		
		if (portnode == this)
		{
			*pface = LinkFace::INNER;
		}
		else if (portnode->get_parent() == this)
		{
			*pface = LinkFace::OUTER;
		}
		else
		{
			throw HierException(port, "can't connect, not within bounds of node " + get_hier_path());
		}		
	}

	// Get the respective endpoints
	src_ep = src_port->get_endpoint(nettype, src_face);
	sink_ep = sink_port->get_endpoint(nettype, sink_face);

	// Validate that both src/sink eps exist are connectable with the given nettype
	if (!src_ep || !sink_ep)
	{
		std::string netname = Network::to_string(nettype);
		std::string who_role = !src_ep ? " source" : " sink";
		HierObject* who_obj = !src_ep ? src_port : sink_port;
		throw HierException(who_obj, "not a " + netname + who_role);
	}

	// Finally, swap src/sink if they were given in the wrong order
	if (src_ep->get_dir() == Dir::IN && sink_ep->get_dir() == Dir::OUT)
	{
		std::swap(src, sink);
		std::swap(src_ep, sink_ep);
	}
}

Link* Node::connect(HierObject* src, HierObject* sink, NetType net)
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

NetType Node::find_auto_net_type(HierObject* src, HierObject* sink) const
{
	// Use the Port's nominal net type, and make sure both ports have it
	NetType result = src->locate_port(Dir::OUT)->get_type();
	if (sink->locate_port(Dir::IN)->get_type() != result)
		result = NET_INVALID;

	return result;

	/*
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
	*/
}

Link* Node::connect(HierObject* src, HierObject* sink)
{
	NetType nettype = find_auto_net_type(src, sink);

	if (nettype == NET_INVALID)
		throw Exception("could not automatically determine network type to connect on");

	return connect(src, sink, nettype);
}

void Node::disconnect(HierObject* src, HierObject* sink, NetType nettype)
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

void Node::disconnect(Link* link)
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

	// Don't leave zero-size vectors around
	if (m_links[nettype].empty())
		m_links.erase(nettype);
}

void Node::disconnect(HierObject* src, HierObject* sink)
{
	NetType nettype = find_auto_net_type(src, sink);

	if (nettype == NET_INVALID)
		throw Exception("could not automatically determine network type to disconnect on");

	disconnect(src, sink, nettype);
}

Link* Node::splice(Link* orig, HierObject* new_sink, HierObject* new_src)
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
	get_eps(new_src, new_sink, net, new_src_ep, new_sink_ep);

	// Disconnect old link from its sink and attach it to new sink
	orig->set_sink(new_sink_ep);
	orig_sink_ep->remove_link(orig);
	new_sink_ep->add_link(orig);

	// Create a link from new src to old sink
	Link* new_link = orig->clone();
	new_link->set_src(new_src_ep);
	new_link->set_sink(orig_sink_ep);
	new_src_ep->add_link(new_link);
	orig_sink_ep->add_link(new_link);
	
	// Add link to the system
	m_links[net].push_back(new_link);

	return new_link;
}

void Node::write_dot(const std::string& filename, NetType nettype)
{
	// Open file, create top-level graph
	std::ofstream out(filename + ".dot");
	out << "digraph {" << std::endl;

	// For top-level system graph: create edge for every link
	auto links = get_links(nettype);
	for (auto& link : links)
	{
		Port* src_port = link->get_src();
		Port* sink_port = link->get_sink();

		Node* src_node = src_port->get_node();
		Node* sink_node = sink_port->get_node();

		std::string taillabel = src_port->get_hier_path(src_node);
		std::string headlabel = sink_port->get_hier_path(sink_node);

		std::string attrs = " [headlabel=\"" + headlabel + "\", taillabel=\"" + taillabel + "\"]";

		out << "\"" << src_node->get_name()
			<< "\" -> \"" << sink_node->get_name()
			<< "\"" << attrs << ";" 
			<< std::endl;
	}

	// Finish main graph
	out << "}" << std::endl;
	out.close();
}

void Node::do_post_carriage()
{
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

void System::delete_object(const HierPath& path)
{
	HierObject* obj = remove_child(path);
	delete obj;
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
	static std::string NAME(genie::hier_make_reserved_name("root"));
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

void System::do_post_carriage()
{
	// Forward event to child nodes
	auto nodes = get_nodes();
	for (auto& node : nodes)
	{
		node->do_post_carriage();
	}
}

//
// NodeHDLInfo
//

NodeHDLInfo::NodeHDLInfo()
	: m_node(nullptr)
{
}