#include "genie/structure.h"
#include "genie/hierarchy.h"

using namespace genie;

//
// PortDef
//

PortDef::PortDef(Dir dir)
: m_dir(dir)
{
	// AInstantiable added by derived classes
}

PortDef::PortDef(Dir dir, const std::string& name, HierObject* parent)
: PortDef(dir)
{
	set_name(name);
	set_parent(parent);
}

//
// NodeDef
//

NodeDef::NodeDef()
{
	asp_add(new AHierParent(this));
}

NodeDef::NodeDef(const std::string& name, HierObject* parent)
: NodeDef()
{
	set_name(name);
	set_parent(parent);
}

NodeDef::PortDefs NodeDef::get_port_defs() const
{
	auto ap = asp_get<AHierParent>();
	return ap->get_children_by_type<PortDef>();
}

PortDef* NodeDef::get_port_def(const std::string& name) const
{
	auto ap = asp_get<AHierParent>();
	return dynamic_cast<PortDef*>(ap->get_child(name));
}

void NodeDef::add_port_def(PortDef* def)
{
	auto ap = asp_get<AHierParent>();
	ap->add_child(def);
}

void NodeDef::remove_port_def(const std::string& name)
{
	auto ap = asp_get<AHierParent>();
	ap->remove_child(name);
}

//
// Port
//

Port::Port(Dir dir)
: m_dir(dir)
{
	// AInstance and AEndpoints added by derived classes
}

Port::Port(Dir dir, const std::string& name, HierObject* parent)
: Port(dir)
{
	set_name(name);
	set_parent(parent);
}

//
// Export
//

Export::Export(Dir dir)
: m_dir(dir)
{
}

Export::Export(Dir dir, const std::string& name, System* parent)
: Export(dir)
{
	set_name(name);
	set_parent(parent);
}

Export::~Export()
{
}

//
// Node
//

Node::Node()
{
	asp_add(new AHierParent(this));
}

Node::Node(const std::string& name, HierObject* parent)
: Node()
{
	set_name(name);
	set_parent(parent);
}

Node::Ports Node::get_ports(NetType net) const
{
	auto ai = asp_get<AHierParent>();
	return ai->get_children<Port>([=] (const HierObject* o)
	{
		const Port* p = dynamic_cast<const Port*>(o);
		return p && p->get_type() == net;
	});
}

Node::Ports Node::get_ports() const
{
	auto ai = asp_get<AHierParent>();
	return ai->get_children_by_type<Port>();
}

Port* Node::get_port(const std::string& name) const
{
	auto ai = asp_get<AHierParent>();
	return as_a<Port*>(ai->get_child(name));
}

void Node::add_port(Port* p)
{
	auto ai = asp_get<AHierParent>();
	ai->add_child(p);
}

void Node::delete_port(const std::string& name)
{
	auto ai = asp_get<AHierParent>();
	HierObject* p = ai->remove_child(name);
	delete p;
}


//
// System
//

System::System()
{
	// A system contains things
	asp_add(new AHierParent(this));
}

HierObject* System::instantiate()
{
	// Create a new Node
	Node* result = new Node(this->get_name());
			
	// Give it a reference back to the System that instantiated it
	result->set_prototype(this);

	// Go through all the System's exports and Instantiate them in the new SystemNode
	auto exports = this->get_exports();
	for (auto& exp : exports)
	{
		auto port = as_a<Port*>(exp->instantiate());
		assert(port);

		result->add_port(port);
	}

	return result;
}

System::System(const std::string& name)
: System()
{
	set_name(name);
}

System::~System()
{
	// Clean up links
	for (auto& links : m_links)
	{
		// m_links is a map of Lists. here we delete each List
		Util::delete_all(links.second);
	}
}

System::NetTypes System::get_net_types() const
{
	return Util::keys<NetTypes>(m_links);
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
	AEndpoint* src_ep;
	AEndpoint* sink_ep;

	get_eps(src, sink, nettype, src_ep, sink_ep);

	// Go to source endpoint, do linear search through all outgoing links 
	for (auto& link : src_ep->links())
	{
		if (link->get_sink() == sink_ep)
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
		throw Exception("Could not automatically determine network type");

	return get_links(src, sink, nettype);
}

void System::get_eps(HierObject* src, HierObject* sink, NetType nettype,
	AEndpoint*& src_ep, AEndpoint*& sink_ep) const
{
	// Get information about the network type we're going to be connecting on
	NetTypeDef* def = NetTypeDef::get(nettype);
	AspectID ep_asp_id = def->get_ep_asp_id();

	src_ep = (AEndpoint*)src->asp_get(ep_asp_id);
	sink_ep = (AEndpoint*)sink->asp_get(ep_asp_id);

	if (!src_ep || !sink_ep)
	{
		std::string netname = def->get_name();
		std::string who_role = !src_ep ? " source" : " sink";
		HierObject* who_obj = !src_ep ? src : sink;
		throw HierException(who_obj, "not a " + netname + who_role);
	}
}

Link* System::connect(HierObject* src, HierObject* sink, NetType net)
{
	NetTypeDef* def = NetTypeDef::get(net);
	AEndpoint* src_ep;
	AEndpoint* sink_ep;

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
	NetType nettype = NET_INVALID;

	for (auto obj : { src, sink })
	{
		auto asps = obj->asp_get_all_matching<AEndpoint>();

		// Must have exactly one aspect that derives from AEndpoint
		if (asps.size() == 1)
		{
			nettype = asps.front()->get_type();
			break;
		}
	}

	return nettype;
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
	AEndpoint* src_ep;
	AEndpoint* sink_ep;

	get_eps(src, sink, nettype, src_ep, sink_ep);

	// Go through src's links, find the one whose sink is 'sink'
	for (auto link : src_ep->links())
	{
		if (link->get_sink() == sink_ep)
		{
			// Found it. Disconnect and get out before we break the for loop
			disconnect(link);
			break;
		}
	}
}

void System::disconnect(Link* link)
{
	AEndpoint* src_ep = link->get_src();
	AEndpoint* sink_ep = link->get_sink();
	NetType nettype = link->get_type();

	// Disconnect the Link from its endpoints
	src_ep->remove_link(link);
	sink_ep->remove_link(link);

	// Remove the link from the System and destroy it
	Util::erase(m_links[nettype], link);
	delete link;
}

void System::disconnect(HierObject* src, HierObject* sink)
{
	NetType nettype = find_auto_net_type(src, sink);

	if (nettype == NET_INVALID)
		throw Exception("Could not automatically determine network type to disconnect on");

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
		throw Exception("Can not splice link: has child links");

	AEndpoint* orig_src_ep = orig->get_src();
	AEndpoint* orig_sink_ep = orig->get_sink();
	NetType net = orig->get_type();

	AEndpoint* new_sink_ep;
	AEndpoint* new_src_ep;
	get_eps(new_sink, new_src, net, new_sink_ep, new_src_ep);

	// Disconnect old link from its sink and attach it to new sink
	orig->set_sink(new_sink_ep);
	orig_sink_ep->remove_link(orig);
	new_sink_ep->add_link(orig);

	// Create a link from new src to old sink
	Link* new_link = NetTypeDef::get(net)->create_link();
	new_link->set_src(new_src_ep);
	new_link->set_sink(new_sink_ep);
	new_src_ep->add_link(new_link);
	new_sink_ep->add_link(new_link);

	// Copy parent containment
	if (acont)
	{
		auto newcont = new_link->asp_add(new ALinkContainment(new_link));
		auto parents = acont->get_parent_links();
		for (auto parent : parents)
		{
			newcont->add_parent_link(parent);
		}
	}
}

System::Objects System::get_objects() const
{
	auto ap = asp_get<AHierParent>();
	return ap->get_children();
}

System::Nodes System::get_nodes() const
{
	auto ap = asp_get<AHierParent>();
	return ap->get_children_by_type<Node>();
}

System::Exports System::get_exports() const
{
	auto ap = asp_get<AHierParent>();
	return ap->get_children_by_type<Export>();
}

void System::add_object(HierObject* obj)
{
	// System's Parent aspect
	auto ap = asp_get<AHierParent>();
	ap->add_child(obj);
}

HierObject* System::get_object(const HierPath& path) const
{
	auto ap = asp_get<AHierParent>();
	return ap->get_child(path);
}

void System::delete_object(const HierPath& path)
{
	auto ap = asp_get<AHierParent>();
	HierObject* obj = ap->remove_child(path);
	delete obj;
}

//
// HierRoot
//

HierRoot::HierRoot()
{
	asp_add(new AHierParent(this));
}

HierRoot::~HierRoot()
{
}

const std::string& HierRoot::get_name() const
{
	static std::string NAME("<root>");
	return NAME;
}

void HierRoot::add_object(HierObject* obj)
{
	auto ap = asp_get<AHierParent>();
	ap->add_child(obj);
}

HierObject* HierRoot::get_object(const HierPath& path)
{
	auto ap = asp_get<AHierParent>();
	return ap->get_child(path);
}

HierRoot::Systems HierRoot::get_systems()
{
	auto ap = asp_get<AHierParent>();
	return ap->get_children_by_type<System>();
}

HierRoot::NodeDefs HierRoot::get_node_defs()
{
	auto ap = asp_get<AHierParent>();
	return ap->get_children_by_type<NodeDef>();
}

HierRoot::Objects HierRoot::get_objects()
{
	auto ap = asp_get<AHierParent>();
	return ap->get_children();
}