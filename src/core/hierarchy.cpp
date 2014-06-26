#include "ct/hierarchy.h"

using namespace ct;

static const char PATH_SEP = '.';

//
// HierNode
//

HierNode::Children HierNode::hier_children(const HierNode::FilterFunc& filter) const
{
	// Get all children and erase those that don't match the filter
	Children result = hier_children();
	result.erase(std::remove_if(result.begin(), result.end(), std::not1(filter)));
	return result;
}

HierPath HierNode::hier_full_name() const
{
	// Start with our name
	HierPath result = hier_name();
	
	// Go up the hierarchy and append names of parents (when not empty)
	for (HierNode* p = hier_parent(); p != nullptr; p = p->hier_parent())
	{	
		std::string pname = p->hier_name();
		if (!pname.empty())
			result = pname + PATH_SEP + result;
	}

	return result;
}

HierNode* HierNode::hier_node(const std::string& name) const
{
	// Find hierarchy separator and get first name fragment
	size_t spos = name.find_first_of(PATH_SEP);
	std::string frag = name.substr(0, spos);

	// Find direct child
	HierNode* node = hier_child(frag);
	
	// If this isn't the last name fragment in the hierarchy, recurse
	if (node && spos != name.npos)
	{
		node = node->hier_child(frag.substr(spos));
	}

	return node;
}

//
// HierContainer
//

HierContainer::HierContainer(const std::string& name, HierNode* parent)
: m_name(name), m_parent(parent)
{
}

HierContainer::~HierContainer()
{
	Util::delete_all_2(m_nodes);
}

const std::string& HierContainer::hier_name() const
{
	return m_name;
}

HierNode* HierContainer::hier_parent() const
{
	return m_parent;
}

HierNode* HierContainer::hier_child(const std::string& n) const
{
	auto it = m_nodes.find(n);
	if (it == m_nodes.end())
		throw Exception("Child object " + n + " not found");

	return it->second;
}

HierNode::Children HierContainer::hier_children() const
{
	return Util::values<Children>(m_nodes);
}

void HierContainer::hier_add(HierNode* node)
{
	const std::string& n = node->hier_name();
	if (m_nodes.count(n))
		throw Exception("Child object " + n + " + already exists");

	m_nodes[n] = node;
}