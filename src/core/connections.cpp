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
		throw Exception("link is already bound to endpoint");

	// Check connection rules
	NetTypeDef* def = get_net_def();

	if (is_connected())
	{
		if (m_dir == Dir::OUT && !def->get_src_multibind())
			throw HierException(asp_container(), "source endpoint does not support multiple bindings");
		else if (m_dir == Dir::IN && !def->get_sink_multibind())
			throw HierException(asp_container(), "sink endpoint does not support multiple bindings");
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
{
	set_src(src);
	set_sink(sink);
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
	if (ep)
	{
		if (m_src)
			throw HierException(ep->asp_container(), "link src already set");

		if (ep->get_dir() != Dir::OUT)
			throw HierException(ep->asp_container(), "tried to use sink endpoint as a source");
	}

	m_src = ep;
}

void Link::set_sink(AEndpoint* ep)
{
	if (ep)
	{
		if (m_sink)
			throw HierException(ep->asp_container(), "link sink already set");

		if (ep->get_dir() != Dir::IN)
			throw HierException(ep->asp_container(), "tried to use source endpoint as a sink");
	}

	m_sink = ep;
}

NetType Link::get_type() const
{
	// For now, return the network type of one of the endpoints rather than keeping
	// our own m_type field in the link itself
	AEndpoint* ep = m_src? m_src : m_sink;
	if (!ep)
		throw Exception("link not connected to anything, can not determine network type");

	return ep->get_type();
}

//
// ALinkContainment
//

ALinkContainment::ALinkContainment(Link* container)
: AspectWithRef(container)
{
}

ALinkContainment::~ALinkContainment()
{
}




void ALinkContainment::add_link(Link* other, PorC porc)
{
	assert(other);

	// Add to our list
	this->add_link_internal(other, porc);
	
	// Update the other link
	auto other_containment = other->asp_get<ALinkContainment>();
	if (other_containment)
	{
		Link* thislink = this->asp_container();
		other_containment->add_link_internal(thislink, rev_rel(porc));
	}
}

Link* ALinkContainment::get_link0(PorC porc) const
{
	const Links& links = get_links(porc);
	return links.empty() ? nullptr : links.front();
}

void ALinkContainment::remove_link(Link* other, PorC porc)
{
	assert(other);

	// Tell other that we're no longer associated
	auto other_containment = other->asp_get<ALinkContainment>();
	if (other_containment)
	{
		Link* thislink = this->asp_container();
		other_containment->remove_link_internal(thislink, rev_rel(porc));
	}

	// Now disassociate ourselves from the other link
	this->remove_link_internal(other, porc);
}

void ALinkContainment::remove_link_internal(Link* other, PorC porc)
{
	Util::erase(m_links[porc], other);
}

void ALinkContainment::add_link_internal(Link* other, PorC porc)
{
	Links& links = m_links[porc];

	// Only insert if not existing
	auto it = std::find(links.begin(), links.end(), other);
	if (it == links.end())
		links.insert(it, other);
}

ALinkContainment::Links ALinkContainment::get_all_links(NetType type, PorC porc) const
{
	Links result;

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

		// Iterate over parents (or children) of currently visited link
		for (Link* other : containment->get_links(porc))
		{
			// Matches type - add to final return list
			if (other->get_type() == type)
				result.push_back(other);

			// Match or no match, add it to the stack for traversal
			to_visit.push(other);
		}
	}

	return result;
}
