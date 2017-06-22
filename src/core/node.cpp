#include "pch.h"
#include "node.h"
#include "params.h"
#include "port_clockreset.h"
#include "port_conduit.h"
#include "port_rs.h"
#include "genie_priv.h"

using namespace genie::impl;
using genie::Exception;

namespace
{
    // Concrete ParamResolver implementation for Nodes
    class NodeParamResolver : public ParamResolver
    {
    protected:
        Node* n;
    public:
        NodeParamResolver(Node* _n): n(_n) {}
        NodeParam* resolve(const std::string& name) override
        {
            // Look for parameter in parent node first before
            // checking this node. Do not recurse further.
            NodeParam* result = nullptr;
            Node* p = n->get_parent_node();
			if (p)
				result = p->get_param(name);

            if (!result)
                result = n->get_param(name);

            if (!result)
                throw Exception(n->get_hier_path() + ": unresolved parameter " + name);

            return result;
        }
    };

    // Used for clock and reset ports
    template<class P>
    Port * create_simple_port(Node* node, const std::string & name, genie::Port::Dir dir, 
        const std::string & hdl_sig)
    {
        // Create the port
        auto result = new P(name, dir);

        // Create HDL port
        auto hdl_port =	node->get_hdl_state().add_port(hdl_sig, 1, 1, hdl::Port::Dir::from_logical(dir));

        // Create binding (using defaults for 1-bit binding)
        hdl::PortBindingRef binding;
        binding.set_port_name(hdl_sig);
        result->set_binding(binding);

        // Add port to hierarchy
        node->add_child(result);

        return result;
    }
}

//
// Public functions
//

void Node::set_int_param(const std::string& parm_name, int val)
{
	set_param(parm_name, new NodeIntParam(val));
}

void Node::set_int_param(const std::string& parm_name, const std::string & expr)
{
	try
	{
		set_param(parm_name, new NodeIntParam(expr));
	}
	catch (Exception& e)
	{
		throw Exception("Parameter " + parm_name + " of " + get_hier_path() + ": " + e.what());
	}
}

void Node::set_str_param(const std::string & parm_name, const std::string & str)
{
	set_param(parm_name, new NodeStringParam(str));
}

void Node::set_lit_param(const std::string & parm_name, const std::string & str)
{
    set_param(parm_name, new NodeLiteralParam(str));
}

genie::Port * Node::create_clock_port(const std::string & name, genie::Port::Dir dir, 
    const std::string & hdl_sig)
{
	return create_simple_port<PortClock>(this, name, dir, hdl_sig);
}

genie::Port * Node::create_reset_port(const std::string & name, genie::Port::Dir dir, 
    const std::string & hdl_sig)
{
    return create_simple_port<PortReset>(this, name, dir, hdl_sig);
}

genie::PortConduit * Node::create_conduit_port(const std::string & name, genie::Port::Dir dir)
{
    auto result = new PortConduit(name, dir);
    add_child(result);
    return result;
}

genie::PortRS * Node::create_rs_port(const std::string & name, genie::Port::Dir dir, 
    const std::string & clk_port_name)
{
    auto result = new PortRS(name, dir);
    result->set_clock_port_name(clk_port_name);
	add_child(result);
    return result;
}



//
// Internal functions
//

Node::~Node()
{
	// Clean up links
	for (auto& link_cont : m_links)
	{
		if (link_cont.get_type() != NET_INVALID)
		{
			auto links = link_cont.get_all();
			util::delete_all(links);
		}
	}
}

Node::Node(const std::string & name, const std::string & hdl_name)
    : m_hdl_name(hdl_name), m_hdl_state(this)
{
	set_name(name);
}

Node::Node(const Node& o)
    : HierObject(o), m_hdl_name(o.m_hdl_name),
    m_hdl_state(o.m_hdl_state)
{
    // Copy over all the things that every Node has

	// Parameters and values
	for (auto it : o.m_params)
	{
		m_params[it.first] = it.second->clone();
	}

    // Point HDL state back at us
    m_hdl_state.set_node(this);

	// Ports
	for (auto p : o.get_children_by_type<Port>())
	{
		auto p_copy = p->clone();
		add_child(p_copy);
	}
}

void Node::reintegrate(HierObject *obj)
{
	// Reintegrate recursively
	for (auto our_child : this->get_children())
	{
		auto their_child = obj->get_child_as<HierObject>(our_child->get_name());
		if (their_child)
		{
			our_child->reintegrate(their_child);
		}
	}

}

Node * Node::get_parent_node() const
{
	return dynamic_cast<Node*>(get_parent());
}

void Node::resolve_size_params()
{
	NodeParamResolver resolv(this);

	// Resolve expresisons within child ports
	auto ports = get_children_by_type<Port>();
	for (auto p : ports)
	{
		p->resolve_params(resolv);
	}

	// Resolve expressions within HDL-related stuff
	m_hdl_state.resolve_params(resolv);
}

void Node::resolve_all_params()
{
    NodeParamResolver resolv(this);

    // First, concreteize any expressions within the parameters themselves
    for (auto& p : m_params)
    {
        p.second->resolve(resolv);
    }

	resolve_size_params();
}


NodeParam* Node::get_param(const std::string & name)
{
    auto it = (m_params.find(name));
    if (it == m_params.end())
    {
        return nullptr;
    }
    else
    {
        return it->second;
    }   
}

void Node::set_bits_param(const std::string parm_name, const BitsVal & val)
{
	set_param(parm_name, new NodeBitsParam(val));
}

Port* Node::add_port(Port* p)
{
	add_child(p);
	return p;
}

Node::Links Node::get_links() const
{
	Links result;
	for (auto& cont : m_links)
	{
		auto links = cont.get_all();
		result.insert(result.end(), links.begin(), links.end());
	}
	return result;
}

Node::Links Node::get_links(NetType type)
{
	auto& cont = get_links_cont(type);
	return cont.get_all();
}

Node::Links Node::get_links(HierObject* src, HierObject* sink, NetType nettype) const
{
	Links result;
	Endpoint* src_ep;
	Endpoint* sink_ep;

	src_ep = src->get_endpoint(nettype, Port::Dir::OUT);
	sink_ep = sink->get_endpoint(nettype, Port::Dir::IN);

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

Link * Node::get_link(LinkID id)
{
	auto& cont = get_links_cont(id.get_type());
	return cont.get(id);
}

LinkID Node::add_link(NetType type, Link* link)
{
	auto& cont = get_links_cont(type);
	return cont.insert_new(link);
}


Link * Node::connect(HierObject * src, HierObject * sink, NetType net)
{
	const NetworkDef* def = impl::get_network(net);
	
	Endpoint* src_ep = src->get_endpoint(net, Port::Dir::OUT);
	Endpoint* sink_ep = sink->get_endpoint(net, Port::Dir::IN);

	// Do some validation
	for (auto obj_ep : { std::make_pair(src, src_ep), std::make_pair(sink, sink_ep) })
	{
		auto obj = obj_ep.first;
		auto ep = obj_ep.second;

		// Src and sink must both be children of this Node
		if (!this->is_parent_of(obj))
		{
			throw Exception(get_hier_path() + ": can't connect " +
				src->get_hier_path() + " to " + sink->get_hier_path() +
				" because " + obj->get_hier_path() + " is not a child object");
		}

		// Check if connectable
		if (!ep)
		{
			throw Exception(get_hier_path() + ": can't connect " +
				src->get_hier_path() + " to " + sink->get_hier_path() +
				" because " + obj->get_hier_path() +
				" does not accept connections of type " +
				def->get_name());
		}

		// Furthermore: if the object is a Port, then we are connecting to the side
		// of it that lies inside the system.
		Port* port = dynamic_cast<Port*>(obj);
		if (port)
		{
			auto eff_dir = port->get_effective_dir(this);
			auto used_dir = ep->get_dir();
			if (eff_dir != used_dir)
			{
				std::string srcsink = used_dir == Port::Dir::IN ? "sink" : "src";
				throw Exception(port->get_hier_path() + ": port is not a " + srcsink +
					" with respect to " + this->get_hier_path());
			}
		}
	}

	// Check if a link already exists, and return it if so
	for (auto existing_link : src_ep->links())
	{
		if (existing_link->get_sink_ep() == sink_ep)
		{
			return existing_link;
		}
	}

	// Create link and set its src/sink
	Link* link = def->create_link();
	link->set_src_ep(src_ep);
	link->set_sink_ep(sink_ep);

	// Hook up endpoints to the link
	src_ep->add_link(link);
	sink_ep->add_link(link);

	// Add the link to the system
	add_link(net, link);

	return link;
}

void Node::disconnect(HierObject* src, HierObject* sink, NetType nettype)
{
	Endpoint* src_ep = src->get_endpoint(nettype, Port::Dir::OUT);
	Endpoint* sink_ep = sink->get_endpoint(nettype, Port::Dir::IN);

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
	LinkID id = link->get_id();

	// Disconnect the Link from its endpoints
	src_ep->remove_link(link);
	sink_ep->remove_link(link);

	// Remove link from relations
	m_link_rel.unregister_link(link->get_id());

	// Remove the link from the System and destroy it
	auto& cont = get_links_cont(id.get_type());
	cont.remove(id);
	delete link;
}

Link * Node::splice(Link * orig, HierObject * new_sink, HierObject * new_src)
{
	//
	// orig_src --> orig --> orig_sink
	//
	// becomes
	//
	// orig_src --> orig --> new_sink, new_src --> (new link) --> orig_sink
	//

	Endpoint* orig_src_ep = orig->get_src_ep();
	Endpoint* orig_sink_ep = orig->get_sink_ep();
	NetType net = orig->get_type();

	Endpoint* new_sink_ep = new_sink->get_endpoint(net, Port::Dir::IN);
	Endpoint* new_src_ep = new_src->get_endpoint(net, Port::Dir::OUT);

	// Disconnect old link from its sink and attach it to new sink
	orig->set_sink_ep(new_sink_ep);
	orig_sink_ep->remove_link(orig);
	new_sink_ep->add_link(orig);

	// Create a link from new src to old sink
	Link* new_link = orig->clone();
	new_link->set_src_ep(new_src_ep);
	new_link->set_sink_ep(orig_sink_ep);
	new_src_ep->add_link(new_link);
	orig_sink_ep->add_link(new_link);

	// Add link to the system
	add_link(net, new_link);

	// Transfer containment relationships. Just immediate parents
	auto old_parents = m_link_rel.get_immediate_parents(orig->get_id());

	for (auto parent : old_parents)
	{
		m_link_rel.add(parent, new_link->get_id());
	}

	return new_link;
}

bool Node::is_link_internal(Link* link) const
{
	// The endpoints must be Ports belonging to this Node
	auto src_port = dynamic_cast<Port*>(link->get_src());
	auto sink_port = dynamic_cast<Port*>(link->get_sink());

	if (!src_port || !sink_port)
		return false;

	if (src_port->get_node() != this ||
		sink_port->get_node() != this)
	{
		return false;
	}

	return true;
}

//
// Protected functions
//

void Node::set_param(const std::string& name, NodeParam * param)
{
	// Uppercase the name
	std::string name_up = util::str_toupper(name);

	// Check for existing param
	auto it = m_params.find(name_up);
	if (it != m_params.end())
	{
		auto old_param = it->second;

		// Check if new param's value is of different type than existing and throw warning
		if (param->get_type() != old_param->get_type())
		{
			log::warn("%s: redefining parameter %s with different basic type",
				this->get_hier_path().c_str(), name.c_str());
		}
		
		delete old_param;
	}

	m_params[name] = param;
}

void Node::copy_links_from(const Node & src, const Links* just_these)
{
	// Prepare link containers
	for (auto& other_cont : src.m_links)
	{
		auto& cont = get_links_cont(other_cont.get_type());
		cont.prepare_for_copy(other_cont);
	}

	// If just_these is not null, we'll copy that list. Otherwise,
	// we need to create a temporary vector of ALL links
	Links* all_links = nullptr;
	if (!just_these)
		all_links = new Links(src.get_links());

	auto& links = just_these ? *just_these : *all_links;
	for (Link* orig_link : links)
	{
		HierObject* orig_src = orig_link->get_src();
		HierObject* orig_sink = orig_link->get_sink();

		// Use names of original link endpoints to locate the copies of those
		// endpoints in the cloned system.
		HierObject* new_src = dynamic_cast<HierObject*>(this->get_child(orig_src->get_hier_path(&src)));
		HierObject* new_sink = dynamic_cast<HierObject*>(this->get_child(orig_sink->get_hier_path(&src)));

		// If either cloned endpoint is not found, just ignore the link
		if (!new_src || !new_sink)
			continue;

		// Make new link
		auto new_link = orig_link->clone();

		// Find endpoints in target(this) context, connect
		auto new_src_ep = new_src->get_endpoint(orig_link->get_type(), Port::Dir::OUT);
		auto new_sink_ep = new_sink->get_endpoint(orig_link->get_type(), Port::Dir::IN);

		new_src_ep->add_link(new_link);
		new_sink_ep->add_link(new_link);

		new_link->set_src_ep(new_src_ep);
		new_link->set_sink_ep(new_sink_ep);

		// Do an add-in-place using existing ID
		auto& cont = get_links_cont(orig_link->get_type());
		cont.insert_existing(new_link);
	}

	// Destroy temp copy of all links if we created one
	if (all_links)
		delete all_links;
}

LinksContainer & Node::get_links_cont(NetType type)
{
	NetType cur_max = (NetType)m_links.size();

	if (type >= cur_max)
	{
		m_links.resize(type + 1);
		for (auto i = cur_max; i <= type; i++)
		{
			m_links[i].set_type(i);
		}
	}

	return m_links[type];
}


