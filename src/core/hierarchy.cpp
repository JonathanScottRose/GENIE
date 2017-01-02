#include "pch.h"
#include "hierarchy.h"
#include "util.h"

using namespace genie::impl;

// Default name, should help with debugging
static const char* UNNAMED_OBJECT = "<unnamed object>";

// Regex pattern for legal object names
// Alphanumeric characters plus underscore. First character can't be a number or underscore,
// except for internal reserved names.
static const char* LEGAL_NAME = "[a-zA-Z][0-9a-zA-Z_]*";

//
// Globals
//

//std::string genie::hier_make_reserved_name(const std::string& name)
//{
//	return "_" + name;
//}
//
//HierPath genie::hier_path_append(const HierPath& a, const HierPath& b)
//{
//	return a + PATH_SEP + b;
//}
//
//std::string genie::hier_path_collapse(const HierPath& path)
//{
//	std::string result = path;
//	std::replace(result.begin(), result.end(), PATH_SEP, '_');
//	return result;
//}

//
// HierObject
//

HierObject::HierObject()
	: m_name(UNNAMED_OBJECT), m_parent(nullptr)
{
}

HierObject::~HierObject()
{
	util::delete_all_2(m_children);
}

HierObject::HierObject(const HierObject& o)
	: m_name(o.m_name), m_parent(nullptr)
{
	// Do not copy over the children -- let subclasses decide whether or not to do that.
}

void HierObject::set_name(const std::string& name)
{
	// Static to improve performance?
	static std::regex rgx(LEGAL_NAME);

	if (!std::regex_match(name, rgx))
	{
			throw Exception("invalid object name: '" + name + "'");
	}

	// Remove ourselves from parent
	if (m_parent) m_parent->remove_child(m_name);
	
	// Rename ourselves
	m_name = name;

	// Add back to parent with new name
	if (m_parent) m_parent->add_child(this);
}

const std::string& HierObject::get_name() const
{
	return m_name;
}

std::string HierObject::make_unique_child_name(const std::string & base)
{
    for (unsigned i = 0; ; i++)
    {
        std::string result = base + std::to_string(i);
        if (m_children.count(result) == 0)
            return result;
    }
}

void HierObject::set_parent(HierObject* parent)
{
	if (m_parent)
		throw Exception(get_hier_path() + ": tried to change parent without removing from old one first");

	m_parent = parent;
}

HierObject* HierObject::get_parent() const
{
	return m_parent;
}

HierPath HierObject::get_hier_path(const HierObject* rel_to) const
{
	// For now, HierPath is just a string.
	std::string result = get_name();

	// Construct the path by walking backwards through our parents and adding their names, until
	// a parentless node is found (this is the root, whose name we don't append).
	for (HierObject* cur_obj = get_parent(); cur_obj != nullptr; )
	{
		HierObject* cur_parent = cur_obj->get_parent();

		// If we've hit the 'relative-to' object, or we've hit a root node, then
		// don't append the current object's name, and stop.
		if (cur_obj == rel_to || cur_parent == nullptr)
			break;

		// Append name
		result = cur_obj->get_name() + PATH_SEP + result;

		// Retreat up the hierarchy one level
		cur_obj = cur_parent;
	}

	return result;
}

void HierObject::add_child(HierObject* child_obj)
{
	// Get the child's name, which should have been set by this point.
	const std::string& name = child_obj->get_name();
	if (name == UNNAMED_OBJECT)
		throw Exception(get_hier_path() + " tried to add an unnamed child object");

	// The name should also be unique
	if (m_children.count(name) > 0)
		throw HierDupException(this, child_obj);

	// Add the child to our map of children objects
	m_children[name] = child_obj;

	// Remove the object from an existing parent, if it has one
	if (auto old_parent = child_obj->get_parent())
	{
		old_parent->m_children.erase(child_obj->get_name());
	}

	// Point the child back to us as a parent
	child_obj->set_parent(this);
}

HierObject* HierObject::get_child(const HierPath& path) const
{
	// Find hierarchy separator and get first name fragment
	size_t spos = path.find_first_of(PATH_SEP);
	std::string frag = path.substr(0, spos);

	// Find direct child
	auto itr = m_children.find(frag);
	if (itr == m_children.end())
		return nullptr;

	HierObject* child = itr->second;
	assert(child); // shouldn't have an extant but null entry

	// If this isn't the last name fragment in the hierarchy, recurse
	if (spos != std::string::npos)
	{
		child = child->get_child(path.substr(spos+1));
	}

	return child;
}

bool HierObject::has_child(const HierPath& path) const
{
	return get_child(path) != nullptr;
}

HierObject* HierObject::remove_child(const HierPath& path)
{
	// First get the object (which could be several layers down below us in the hierarchy)
	HierObject* obj = get_child(path);

	if (!obj)
		throw Exception(get_hier_path() + " tried to remove nonexistent child object " + path);

	obj->unlink_from_parent();
	return obj;
}

HierObject* HierObject::remove_child(HierObject* child)
{
	// Retur nullptr if child isn't actually a child of us
	HierObject* result = nullptr;

	if (child->get_parent() == this)
	{
		result = child;
		child->unlink_from_parent();
	}

	return result;
}

void HierObject::unlink_from_parent()
{
	// Next, find its direct parent (not necessarily _this_, but could be lower down in the hierarchy)
	HierObject* parent_obj = get_parent();

	// Remove entry from child map, make sure it's actually the same object, not just an impostor
	// that's named the same
	auto it = parent_obj->m_children.find(get_name());
	if (it->second == this)
	{
		parent_obj->m_children.erase(it);

		// Let child now it no longer has parent
		set_parent(nullptr);
	}
}


//
// Exceptions
//

HierDupException::HierDupException(const HierObject* parent, const HierObject* target)
	: Exception(parent->get_hier_path() + " tried to add duplicate object " + target->get_name()),
	m_parent(parent), m_target(target)
{

}
