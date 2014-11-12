#include "genie/structure.h"
#include "genie/instance.h"

using namespace genie;

namespace
{
	class SystemNode : public Node
	{
	public:
		~SystemNode() { }
	};

	class ASystemInstantiable : public AspectMakeRef<AInstantiable, System>
	{
	public:
		ASystemInstantiable(System* sys) : AspectMakeRef(sys) {	}
		~ASystemInstantiable() { }

		Object* instantiate()
		{
			System* sys = asp_container();

			// Create a new Node
			SystemNode* result = new SystemNode();
			result->set_name(sys->get_name());
			
			// Give it a reference back to the System that instantiated it
			result->asp_add(new AInstance(sys));

			// Go through all the System's exports and Instantiate them in the new SystemNode
			auto exports = sys->get_exports();
			for (auto& exp : exports)
			{
				auto ai = exp->asp_get<AInstantiable>();
				auto port = as_a<Port*>(ai->instantiate());

				result->add_port(port);
			}

			return result;
		}
	};
}

//
// PortDef
//

PortDef::PortDef(Dir dir)
: m_dir(dir)
{
	asp_add(new AHierChild());
	// AInstantiable added by derived classes
}

PortDef::PortDef(Dir dir, const std::string& name, HierObject* parent)
: PortDef(dir)
{
	set_name(name);
	set_parent(parent);
}

PortDef::~PortDef()
{
}

//
// NodeDef
//

NodeDef::NodeDef()
{
	asp_add(new AHierChild());
	asp_add(new AHierParent(this));
}

NodeDef::NodeDef(const std::string& name, HierObject* parent)
: NodeDef()
{
	set_name(name);
	set_parent(parent);
}

NodeDef::~NodeDef()
{
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
	asp_add(new AHierChild());
	// AInstance and AEndpoints added by derived classes
}

Port::Port(Dir dir, const std::string& name, HierObject* parent)
: Port(dir)
{
	set_name(name);
	set_parent(parent);
}

Port::~Port()
{
}

//
// Export
//

Export::Export(Dir dir)
: m_dir(dir)
{
	asp_add(new AHierChild());
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
	asp_add(new AHierChild());
	asp_add(new AHierParent(this));
}

Node::Node(const std::string& name, HierObject* parent)
: Node()
{
	set_name(name);
	set_parent(parent);
}

Node::~Node()
{
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
	// A system is a child of the root
	asp_add(new AHierChild());

	// A system is instantiable
	asp_add<AInstantiable>(new ASystemInstantiable(this));

	// A system contains things
	asp_add(new AHierParent(this));
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

const System::Links& System::get_links(NetType type) const
{
	auto it = m_links.find(type);
	assert(it != m_links.end());
	return it->second;
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
		std::string who = !src_ep ? "Source" : "Sink";
		throw Exception(who + " object is not a " + netname + " endpoint");
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
	NetType nettype = src_ep->get_type();

	assert(nettype == sink_ep->get_type());

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

void HierRoot::add_object(HierObject* obj)
{
	auto ap = asp_get<AHierParent>();
	ap->add_child(obj);
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