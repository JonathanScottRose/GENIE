#include <sstream>
#include <unordered_set>
#include "ct/spec.h"
#include "ct/topo_spec.h"

using namespace ct;
using namespace ct::Spec;


//
// LinkTarget
//

LinkTarget::LinkTarget()
{
}

LinkTarget::LinkTarget(const std::string& path)
{
	std::stringstream strm(path);
	std::getline(strm, m_inst, '.');

	if (!std::getline(strm, m_iface, '.'))
	{
		m_iface = "iface";
	}

	if (!std::getline(strm, m_lp, '.'))
	{
		m_lp = "lp";
	}
}

std::string LinkTarget::get_path() const
{
	std::string result = m_inst;
	result += m_iface.empty()? ".iface" : '.' + m_iface;
	result += m_lp.empty()? ".lp" : '.' + m_lp;

	return result;
}

bool LinkTarget::operator== (const LinkTarget& other) const
{
	return get_path() == other.get_path();
}


//
// Link
//

Link::Link(const LinkTarget& src, const LinkTarget& dest)
	: m_src(src), m_dest(dest)
{
}

Link::~Link()
{
}

//
// SysObject
//

SysObject::SysObject(const std::string& name, Type type, System* parent)
	: m_name(name), m_type(type), m_parent(parent)
{
}

SysObject::~SysObject()
{
}

//
// Instance
//

Instance::Instance(const std::string& name, const std::string& component, System* parent)
	: SysObject(name, INSTANCE, parent), m_component(component)
{
}

Instance::~Instance()
{
}

const Expression& Instance::get_param_binding(const std::string& name)
{
	return m_param_bindings[name];
}

void Instance::set_param_binding(const std::string& name, const Expression& expr)
{
	m_param_bindings[name] = expr;
}

//
// Export
//

Export::Export(const std::string& name, Interface* iface, System* parent)
	: SysObject(name, EXPORT, parent), m_iface(iface)
{
}

Export::~Export()
{
	delete m_iface;
}

//
// System
//

System::System()
{
	m_topo = new TopoGraph();
}

System::~System()
{
	Util::delete_all(m_links);
	Util::delete_all_2(m_objects);
	delete m_topo;
}

void System::add_link(Link* link)
{
	m_links.push_back(link);
	const std::string& label = link->get_label();
	if (!label.empty())
	{
		if (m_links_by_label.count(label))
			throw Exception("Link with label " + label + " already defined");

		m_links_by_label[label] = link;
	}
}

Link* System::get_link(const std::string& label) const
{
	auto it = m_links_by_label.find(label);
	if (it == m_links_by_label.end())
		return nullptr;
	else
		return it->second;
}

Link* System::get_link(const LinkTarget& src, const LinkTarget& dest) const
{
	for (auto link : m_links)
	{
		if (link->get_src() == src && link->get_dest() == dest)
			return link;
	}

	return nullptr;
}

const System::ExclusionGroups& System::exclusion_groups() const
{
	return m_exclusion_groups;
}

void System::add_exclusion_group(const ExclusionGroup& grp)
{
	m_exclusion_groups.push_back(grp);
}

System::ExclusionGroups System::exclusion_groups_for_link(Link* link) const
{
	ExclusionGroups result;

	for (const auto& group : m_exclusion_groups)
	{
		for (const std::string& label : group)
		{
			if (label == link->get_label())
				result.push_back(group);
		}
	}

	return result;
}

const System::LatencyQueries& System::latency_queries() const
{
	return m_latency_queries;
}

void System::add_latency_query(const LatencyQuery& query)
{
	m_latency_queries.push_back(query);
}

void System::add_object(SysObject* inst)
{
	assert(!Util::exists_2(m_objects, inst->get_name()));
	m_objects[inst->get_name()] = inst;
}

SysObject* System::get_object(const std::string& name)
{
	return m_objects[name];
}

Component* System::get_component_for_instance(const std::string& name)
{
	auto inst = (Instance*)get_object(name);
	assert(inst != nullptr);
	assert(inst->get_type() == SysObject::INSTANCE);
	return Spec::get_component(inst->get_component());	
}

Linkpoint* System::get_linkpoint(const LinkTarget& path)
{	
	Component* comp = get_component_for_instance(path.get_inst());
	Interface* iface = comp->get_interface(path.get_iface());
	return iface->get_linkpoint(path.get_lp());
}

const Expression& System::get_param_binding(const std::string& name)
{
	return m_param_bindings[name];
}

void System::set_param_binding(const std::string& name, const Expression& expr)
{
	m_param_bindings[name] = expr;
}