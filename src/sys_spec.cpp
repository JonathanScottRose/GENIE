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

Export::Export(const std::string& name, Interface::Type iface_type, Interface::Dir dir, System* parent)
	: SysObject(name, EXPORT, parent), m_iface_type(iface_type), m_iface_dir(dir)
{
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

