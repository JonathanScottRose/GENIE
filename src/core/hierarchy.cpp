#include "ct/hierarchy.h"

using namespace ct;


HierNode::Children HierNode::hier_children(const HierNode::FilterFunc& filter) const
{
	// Get all children and erase those that don't match the filter
	Children result = hier_children();
	result.erase(std::remove_if(result.begin(), result.end(), std::not1(filter)));
	return result;
}

std::string HierNode::hier_full_name() const
{
	// Start with our name
	std::string result = hier_name();
	
	// Go up the hierarchy and append names of parents (when not empty)
	for (HierNode* p = hier_parent(); p != nullptr; p = p->hier_parent())
	{	
		std::string pname = p->hier_name();
		if (!pname.empty())
			result = pname + "." + result;
	}

	return result;
}

HierNode* HierNode::hier_node(const std::string& name) const
{
	// Find hierarchy separator and get first name fragment
	size_t spos = name.find_first_of('.');
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


