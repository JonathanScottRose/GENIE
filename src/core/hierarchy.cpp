#include "genie/hierarchy.h"
#include "genie/instance.h"
#include "genie/regex.h"

using namespace genie;

// Hierarchy path separator character
static const char PATH_SEP = '.';

// Default name, should help with debugging
static const std::string UNNAMED_CHILD = "UNNAMED_CHILD";

// Similar, used when name is not known
static const std::string UNKNOWN_NAME = "UNKNOWN_NAME";

// Regex pattern for legal object names
// Alphanumeric characters plus underscore. First character can't be a number.
static const std::string LEGAL_NAME = "[a-zA-Z_][0-9a-zA-Z_]*";

//
// HierObject
//

HierObject::HierObject()
{
}

Aspect* HierObject::asp_not_found_handler(const AspectID& id) const
{
	Aspect* result = nullptr;

	// Called when asp_get() fails on a HierObject. Try and look up the aspect in the
	// HierObject's prototype HierObject, if one exists.
	
	// are we an instance of some prototype object? 
	if (asp_has<AInstance>())
	{
		// if so, pass the request to our prototype
		auto aproto = asp_get<AInstance>();
		Object* prototype = aproto->get_prototype();

		// this should exist, right?
		assert(prototype);

		result = prototype->asp_get(id);
	}

	return result;
}

const std::string& HierObject::get_name() const
{
	auto ac = asp_get<AHierChild>();
	return ac ? ac->get_name() : UNKNOWN_NAME;
}

void HierObject::set_name(const std::string& name)
{
	auto ac = asp_get<AHierChild>();
	if (ac) ac->set_name(name);
}

HierObject* HierObject::get_parent() const
{
	auto ac = asp_get<AHierChild>();
	return ac? ac->get_parent() : nullptr;
}

void HierObject::set_parent(HierObject* parent)
{
	auto ac = asp_get<AHierChild>();
	if (ac) ac->set_parent(parent);
}

std::string HierObject::get_full_name() const throw()
{
	// This code may be called from an Exception() constructor, so we absolutely
	// can not throw anything
	try
	{
		auto ac = asp_get<AHierChild>();
		return ac? ac->get_full_name() : UNKNOWN_NAME;
	} 
	catch(std::exception& e)
	{
		return e.what();
	} 
	catch(...)
	{
		return UNKNOWN_NAME;
	}
}

//
// HierFolder
//

HierFolder::HierFolder()
{
	asp_add(new AHierChild());
	asp_add(new AHierParent(this));
}

HierFolder::~HierFolder()
{
}

//
// AHierChild
//

AHierChild::AHierChild(HierObject* container)
: AHierChild(UNNAMED_CHILD, container)
{
}

AHierChild::AHierChild(const std::string& name, HierObject* container)
: AspectWithRef(container)
{
	set_name(name);
}

void AHierChild::set_name(const std::string& name)
{
	// Static to improve performance?
	static std::regex rgx(LEGAL_NAME);

	if (!std::regex_match(name, rgx))
		throw Exception("Invalid object name: '" + name + "'");

	m_name = name;
}

std::string AHierChild::get_full_name() const
{
	// Until HierPath becomes a beefier object and not just an alias for string, we
	// just forward this call to get_full_path.
	return get_full_path();
}

HierPath AHierChild::get_full_path() const
{
	// For now, HierPath is just a string.
	std::string result;

	// Construct the path by walking backwards through our parents and adding their names, until
	// a non-child hierarchy node is reached.
	for (HierObject* cur_obj = asp_container(); cur_obj != nullptr; )
	{
		// If the object doesn't implement HierChild, then we've reached the root
		AHierChild* aspC = cur_obj->asp_get<AHierChild>();
		if (!aspC)
			break;

		// Append name
		result = aspC->get_name() + PATH_SEP + result;

		// Retreat up the hierarchy one level
		cur_obj = aspC->get_parent();
		assert(cur_obj);
	}

	return result;
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
	// Get child aspect
	auto child_asp = child_obj->asp_get<AHierChild>();
	if (!child_asp)
		throw Exception("add_child: Object is not a HierChild");

	// Get the child's name, which should have been set by this point.
	const std::string& name = child_asp->get_name();
	if (name == UNNAMED_CHILD)
		throw Exception("add_child: Tried to add an unnamed child");

	// The name should also be unique
	if (m_children.count(name) > 0)
		throw HierDupException(this->m_container, "Child with name " + name +  " already exists");

	// Add the child to our map of children objects
	m_children[name] = child_obj;

	// Point the child back to us (our container object, that is) as a parent
	child_asp->set_parent(m_container);
}

HierObject* AHierParent::get_child(const HierPath& path) const
{
	// Find hierarchy separator and get first name fragment
	size_t spos = path.find_first_of(PATH_SEP);
	std::string frag = path.substr(0, spos);

	// Find direct child
	auto itr = m_children.find(frag);
	if (itr == m_children.end())
		throw HierNotFoundException(this->m_container, "Hierarchy path " + path + " not found.");

	HierObject* child = itr->second;
	assert(child); // shouldn't have an extant but null entry

	// If this isn't the last name fragment in the hierarchy, recurse
	if (spos != std::string::npos)
	{
		auto aparent = child->asp_get<AHierParent>();
		if (!aparent)
		{
			throw HierException(this->m_container, 
				"Path " + path + " --  intermediate child " + frag + " is not a parent.");
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
	auto obj_ac = obj->asp_get<AHierChild>();
	HierObject* parent_obj = obj_ac->get_parent();

	// Now access the parent object's parent aspect
	auto parent_ap = parent_obj->asp_get<AHierParent>();

	// Remove entry from child map
	parent_ap->m_children.erase(obj_ac->get_name());

	return obj;
}

