#include "genie/hierarchy.h"
#include "genie/regex.h"
#include "genie/connections.h"
#include "genie/structure.h"

using namespace genie;

// Hierarchy path separator character
static const char PATH_SEP = '.';

// Default name, should help with debugging
static const std::string UNNAMED_OBJECT = "<unnamed object>";

// Regex pattern for legal object names
// Alphanumeric characters plus underscore. First character can't be a number or underscore,
// except for internal reserved names.
static const std::string LEGAL_NAME = "[a-zA-Z][0-9a-zA-Z_]*";
static const std::string LEGAL_NAME_RESERVED = "(_|[a-zA-Z])[0-9a-zA-Z_]*";

//
// Globals
//

std::string genie::make_reserved_name(const std::string& name)
{
	return "_" + name;
}

HierPath genie::hier_path_append(const HierPath& a, const HierPath& b)
{
	return a + PATH_SEP + b;
}

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
	: Object(o), m_parent(nullptr)
{
	// Retain connectivity
	for (const auto& i : o.m_endpoints)
	{
		NetType type = i.first;
		Endpoint* ep_outer = get_ep_by_face(i.second, LinkFace::OUTER);
		
		set_connectable(type, ep_outer->get_dir());
	}
}

Endpoint* HierObject::get_ep_by_face(const EndpointsEntry& p, LinkFace f)
{
	switch (f)
	{
	case LinkFace::OUTER: return p.first; break;
	case LinkFace::INNER: return p.second; break;
	default: assert(false);
	}

	return nullptr;
}

void HierObject::set_ep_by_face(EndpointsEntry& p, LinkFace f, Endpoint* ep)
{
	switch (f)
	{
	case LinkFace::OUTER: p.first = ep; break;
	case LinkFace::INNER: p.second = ep; break;
	default: assert(false);
	}
}

HierObject::NetTypes HierObject::get_connectable_networks() const
{
	return util::keys<NetTypes, EndpointsMap>(m_endpoints);
}

bool HierObject::is_connectable(NetType type) const
{
	return m_endpoints.count(type) > 0;
}

bool HierObject::is_connected(NetType type) const
{
	return is_connectable(type) &&
	(
		m_endpoints.at(type).first->is_connected() ||
		m_endpoints.at(type).second->is_connected()
	);
}

Endpoint* HierObject::get_endpoint(NetType type, LinkFace face) const
{
	Endpoint* result = nullptr;

	auto it = m_endpoints.find(type);
	if (it != m_endpoints.end())
	{
		return get_ep_by_face(it->second, face);
	}

	return result;
}

Endpoint* HierObject::get_endpoint(NetType type, HierObject* boundary) const
{
	// Find out whether to use inner or outer face
	const HierObject* obj = this;

	while (obj && !is_a<const Node*>(obj))
		obj = obj->get_parent();

	LinkFace face = (obj == boundary)? LinkFace::INNER : LinkFace::OUTER;
	return get_endpoint(type, face);
}

void HierObject::set_connectable(NetType type, Dir dir)
{
	Network* ndef = Network::get(type);
	assert(type != NET_INVALID);

	if (is_connectable(type))
		throw HierException(this, "already connectable for nettype " + ndef->get_name());

	Endpoint* outer = ndef->create_endpoint(dir);
	Endpoint* inner = ndef->create_endpoint(dir_rev(dir));

	outer->set_obj(this);
	inner->set_obj(this);

	EndpointsEntry entry;
	set_ep_by_face(entry, LinkFace::OUTER, outer);
	set_ep_by_face(entry, LinkFace::INNER, inner);

	m_endpoints.emplace(type, entry);
}

void HierObject::set_unconnectable(NetType type)
{
	m_endpoints.erase(type);
}

void HierObject::set_name(const std::string& name, bool allow_reserved)
{
	// Static to improve performance?
	static std::regex rgx(LEGAL_NAME);
	static std::regex rgxint(LEGAL_NAME_RESERVED);

	if (!std::regex_match(name, rgx))
	{
		if (!allow_reserved || !std::regex_match(name, rgxint))
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

void HierObject::set_parent(HierObject* parent)
{
	m_parent = parent;
}

HierObject* HierObject::get_parent() const
{
	return m_parent;
}

void HierObject::refine(NetType target)
{
	// Default implementation
	auto children = get_children();
	for (auto i : children)
	{
		i->refine(target);
	}
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
		throw HierException(this, "tried to add an unnamed child object");

	// The name should also be unique
	if (m_children.count(name) > 0)
		throw HierDupException(this, "a child object called " + name +  " already exists");

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
		throw HierNotFoundException(this, frag);

	HierObject* child = itr->second;
	assert(child); // shouldn't have an extant but null entry

	// If this isn't the last name fragment in the hierarchy, recurse
	if (spos != std::string::npos)
	{
		child = child->get_child(path.substr(spos+1));
	}

	return child;
}

HierObject* HierObject::remove_child(const HierPath& path)
{
	// First get the object (which could be several layers down below us in the hierarchy)
	HierObject* obj = get_child(path);

	// Next, find its direct parent (not necessarily _this_, but could be lower down in the hierarchy)
	HierObject* parent_obj = obj->get_parent();

	// Remove entry from child map
	parent_obj->m_children.erase(obj->get_name());

	// Let child now it no longer has parent
	obj->set_parent(nullptr);

	return obj;
}

