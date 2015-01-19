#include "genie/hierarchy.h"
#include "genie/regex.h"

using namespace genie;

// Hierarchy path separator character
static const char PATH_SEP = '.';

// Default name, should help with debugging
static const std::string UNNAMED_OBJECT = "<unnamed object>";

// Regex pattern for legal object names
// Alphanumeric characters plus underscore. First character can't be a number.
static const std::string LEGAL_NAME = "[a-zA-Z_][0-9a-zA-Z_]*";

//
// HierObject
//

HierObject::HierObject()
	: m_name(UNNAMED_OBJECT), m_prototype(nullptr), m_parent(nullptr)
{
}

HierObject::HierObject(const std::string& name)
{
	set_name(name);
}

Aspect* HierObject::asp_not_found_handler(const AspectID& id) const
{
	Aspect* result = nullptr;

	// Called when asp_get() fails on a HierObject. Try and look up the aspect in the
	// HierObject's prototype HierObject, if one exists.
	
	// are we an instance of some prototype object? 
	if (m_prototype)
	{
		return m_prototype->asp_get(id);
	}

	return result;
}

void HierObject::set_name(const std::string& name)
{
	// Static to improve performance?
	static std::regex rgx(LEGAL_NAME);

	if (!std::regex_match(name, rgx))
		throw Exception("invalid object name: '" + name + "'");

	// Remove ourselves from parent
	auto ap = m_parent? m_parent->asp_get<AHierParent>() : nullptr;

	if (ap) ap->remove_child(m_name);

	// Rename ourselves
	m_name = name;

	// Add back to parent with new name
	if (ap) ap->add_child(this);
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

void HierObject::set_prototype(HierObject* prototype)
{
	m_prototype = prototype;
}

HierObject* HierObject::get_prototype() const
{
	return m_prototype;
}

HierObject* HierObject::instantiate()
{
	// Default implementation
	return nullptr;
}

std::string HierObject::get_full_name() const
{
	// Until HierPath becomes a beefier object and not just an alias for string, we
	// just forward this call to get_full_path.
	return get_full_path();
}

HierPath HierObject::get_full_path() const
{
	// For now, HierPath is just a string.
	std::string result = get_name();

	// Construct the path by walking backwards through our parents and adding their names, until
	// a parentless node is found (this is the root, whose name we don't append).
	for (HierObject* cur_obj = get_parent(); cur_obj != nullptr; )
	{
		HierObject* cur_parent = cur_obj->get_parent();

		// This object is parentless? We've reached the root. Don't append its name.
		if (!cur_parent)
			break;

		// Append name
		result = cur_obj->get_name() + PATH_SEP + result;

		// Retreat up the hierarchy one level
		cur_obj = cur_parent;
	}

	return result;
}

//
// HierFolder
//

HierFolder::HierFolder()
{
	asp_add(new AHierParent(this));
}

HierFolder::~HierFolder()
{
}

//
// AHierParent
//

AHierParent::AHierParent(HierObject* container)
: m_container(container)
{
}

AHierParent::~AHierParent()
{
	Util::delete_all_2(m_children);
}

void AHierParent::add_child(HierObject* child_obj)
{
	// Get the child's name, which should have been set by this point.
	const std::string& name = child_obj->get_name();
	if (name == UNNAMED_OBJECT)
		throw HierException(m_container, "tried to add an unnamed child object");

	// The name should also be unique
	if (m_children.count(name) > 0)
		throw HierDupException(m_container, "a child object called " + name +  " already exists");

	// Add the child to our map of children objects
	m_children[name] = child_obj;

	// Point the child back to us (our container object, that is) as a parent
	child_obj->set_parent(m_container);
}

HierObject* AHierParent::get_child(const HierPath& path) const
{
	// Find hierarchy separator and get first name fragment
	size_t spos = path.find_first_of(PATH_SEP);
	std::string frag = path.substr(0, spos);

	// Find direct child
	auto itr = m_children.find(frag);
	if (itr == m_children.end())
		throw HierNotFoundException(this->m_container, frag);

	HierObject* child = itr->second;
	assert(child); // shouldn't have an extant but null entry

	// If this isn't the last name fragment in the hierarchy, recurse
	if (spos != std::string::npos)
	{
		auto aparent = child->asp_get<AHierParent>();
		if (!aparent)
		{
			throw HierException(this->m_container, 
				"path " + path + " --  intermediate child " + frag + " is not a parent.");
		}

		child = aparent->get_child(frag.substr(spos));
	}

	return child;
}

HierObject* AHierParent::remove_child(const HierPath& path)
{
	// First get the object
	HierObject* obj = get_child(path);

	// Next, find its direct parent (not necessarily _this_)
	HierObject* parent_obj = obj->get_parent();

	// Now access the parent object's parent aspect
	auto parent_ap = parent_obj->asp_get<AHierParent>();

	// Remove entry from child map
	parent_ap->m_children.erase(obj->get_name());

	// Let child now it no longer has parent
	obj->set_parent(nullptr);

	return obj;
}

