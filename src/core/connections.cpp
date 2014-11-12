#include <stack>
#include "genie/connections.h"
#include "genie/networks.h"
#include "genie/genie.h"

using namespace genie;

//
// Endpoint
//

AEndpoint::AEndpoint(Dir dir, HierObject* container)
: AspectWithRef(container), m_dir(dir)
{
}

AEndpoint::~AEndpoint()
{
	// Connections are owned by someone else. Don't cleanup.
}

NetTypeDef* AEndpoint::get_net_def() const
{
	// Helper method
	return NetTypeDef::get(this->get_type());
}

void AEndpoint::add_link(Link* link)
{
	assert(m_dir != Dir::INVALID);
	
	// Already bound to this specific link
	if (has_link(link))
		throw Exception("Link is already bound to endpoint");

	// Check connection rules
	NetTypeDef* def = get_net_def();

	if (is_connected())
	{
		if (m_dir == Dir::OUT && !def->get_src_multibind())
			throw HierException(asp_container(), "Endpoint does not support multiple source bindings");
		else if (m_dir == Dir::IN && !def->get_sink_multibind())
			throw HierException(asp_container(), "Endpoint does not support multiple sink bindings");
	}

	m_links.push_back(link);
}

void AEndpoint::remove_link(Link* link)
{
	assert (!has_link(link));

	Util::erase(m_links, link);
}

bool AEndpoint::has_link(Link* link) const
{
	return Util::exists(m_links, link);
}

void AEndpoint::remove_all_links()
{
	m_links.clear();
}

const AEndpoint::Links& AEndpoint::links() const
{
	return m_links;
}

Link* AEndpoint::get_link0() const
{
	// Return the first one
	return m_links.empty()? nullptr : m_links.front();
}

AEndpoint::Endpoints AEndpoint::get_remotes() const
{
	Endpoints result;

	// Go through all bound links and collect the things on the other side
	for (auto& link : m_links)
	{
		AEndpoint* ep = link->get_other(this);
		result.push_back(ep);
	}

	return result;
}

AEndpoint* AEndpoint::get_remote0() const
{
	if (m_links.empty())
		return nullptr;

	return m_links.front()->get_other(this);
}

bool AEndpoint::is_connected() const
{
	return !m_links.empty();
}

HierObject* AEndpoint::get_remote_obj0() const
{
	return get_remote0()->asp_container();
}

AEndpoint::Objects AEndpoint::get_remote_objs() const
{
	Objects result;

	for (auto& link : m_links)
	{
		result.push_back(link->get_other(this)->asp_container());
	}

	return result;
}

//
// Link
//



Link::Link(AEndpoint* src, AEndpoint* sink)
: m_src(src), m_sink(sink)
{
}

Link::Link()
: Link(nullptr, nullptr)
{
}

Link::~Link()
{
}

AEndpoint* Link::get_src() const
{
	return m_src;
}

AEndpoint* Link::get_sink() const
{
	return m_sink;
}

AEndpoint* Link::get_other(const AEndpoint* ep) const
{
	assert(ep);

	// Validate
	if (ep == m_src && ep->get_dir() == Dir::OUT)
	{
		return m_sink;
	}
	else if (ep == m_sink && ep->get_dir() == Dir::IN)
	{
		return m_src;
	}
	
	throw Exception("Degenerate link");
	return nullptr; // not reached
}

void Link::set_src(AEndpoint* ep)
{
	if (ep && m_src)
		throw Exception("Link src already set");

	m_src = ep;
}

void Link::set_sink(AEndpoint* ep)
{
	if (ep && m_sink)
		throw Exception("Link sink already set");

	m_sink = ep;
}

NetType Link::get_type() const
{
	// For now, return the network type of one of the endpoints rather than keeping
	// our own m_type field in the link itself
	AEndpoint* ep = m_src? m_src : m_sink;
	if (!ep)
		throw Exception("Link not connected, can not determine network type");

	return ep->get_type();
}

//
// ALinkContainment
//

ALinkContainment::ALinkContainment(Link* container)
: AspectWithRef(container), m_parent_link(nullptr)
{
}

ALinkContainment::~ALinkContainment()
{
}

void ALinkContainment::set_parent_link(Link* parent)
{
	// Already has parent?
	if (m_parent_link)
	{
		// Redundant call, just quit
		if (parent == m_parent_link)
			return;

		// Disconnect from existing parent (if it cares about containment and has an Aspect for it)
		auto parent_containment = m_parent_link->asp_get<ALinkContainment>();
		if (parent_containment)
		{
			// Call the 'internal' version which just removes the child link without
			// also trying to update its parent (us) causing an infinite loop
			parent_containment->remove_child_link_internal(this->asp_container());
		}
	}

	// set our parent to the argument
	m_parent_link = parent;

	// Update new parent if not null
	if (parent)
	{
		auto parent_containment = parent->asp_get<ALinkContainment>();
		if (parent_containment)
		{
			// Call the version which just adds us and doesn't try to call set_parent on us, causing
			// an infinite loop
			parent_containment->add_child_link_internal(this->asp_container());
		}
	}
}

void ALinkContainment::add_child_link(Link* child)
{
	assert(child);

	// Add the child to our list if children
	add_child_link_internal(child);
	
	// Tell child of its new parentage if it cares about such things
	auto child_containment = child->asp_get<ALinkContainment>();
	if (child_containment)
	{
		Link* existing_parent = child_containment->get_parent_link();
		Link* this_link = this->asp_container();

		if (existing_parent && existing_parent != this_link)
			throw Exception("Cannot add child link because it already has a parent");

		child_containment->m_parent_link = this_link;
	}
}

Link* ALinkContainment::get_child_link0() const
{
	return m_child_links.empty() ? nullptr : m_child_links.front();
}

void ALinkContainment::remove_child_link_internal(Link* child)
{
	Util::erase(m_child_links, child);
}

void ALinkContainment::add_child_link_internal(Link* child)
{
	// Only add if it doesn't exist already
	auto it = m_child_links.begin();
	auto it_end = m_child_links.end();
	for ( ; it != it_end; ++it)
	{
		if (*it == child)
			return;
	}

	// Should insert at the end()
	m_child_links.insert(it, child);
}

Link* ALinkContainment::get_parent_link(NetType type) const
{
	Link* parent = m_parent_link;

	// Traverse upwards through parents to find one with the requested type, starting
	// wit this link's parent
	while (parent)
	{
		if (parent->get_type() == type)
		{
			// Found it.
			return parent;
		}

		// Traverse if applicable
		auto parent_containment = parent->asp_get<ALinkContainment>();
		if (parent_containment)
			parent = parent_containment->get_parent_link();
	}

	// Did not find
	return nullptr;
}

ALinkContainment::ChildLinks ALinkContainment::get_child_links(NetType type) const
{
	ChildLinks result;

	// Do a recursive traversal
	std::stack<Link*> to_visit;
	to_visit.push(this->asp_container());

	while (!to_visit.empty())
	{
		Link* link = to_visit.top();
		to_visit.pop();

		// Ignore links which lack containment aspect
		auto containment = link->asp_get<ALinkContainment>();
		if (!containment)
			continue;

		// Check children (this is not a recursive call)
		for (Link* child : containment->get_child_links())
		{
			// Matches type - add to final return list
			if (child->get_type() == type)
				result.push_back(child);

			// Match or no match, add it to the stack for traversal
			to_visit.push(child);
		}
	}

	return result;
}