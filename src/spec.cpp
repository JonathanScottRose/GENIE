#include "spec.h"
#include "common.h"
#include "static_init.h"

using namespace ct;
using namespace Spec;

namespace
{
	typedef std::unordered_map<std::string, Component*> Components;
	Components s_components;
	System s_system;

	Util::InitShutdown s_ctor_dtor([]
	{
	}, []
	{
		Util::delete_all_2(s_components);
	});
}

void Spec::define_component(Component* comp)
{
	assert(!Util::exists_2(s_components, comp->get_name()));
	s_components[comp->get_name()] = comp;
}

Component* Spec::get_component(const std::string& name)
{
	if (s_components.count(name) > 0)
		return s_components[name];
	else
		return nullptr;
}

System* Spec::get_system()
{
	return &s_system;
}

ComponentList Spec::get_all_components()
{
	ComponentList result;
	for (auto& i : s_components)
		result.push_front(i.second);

	return result;
}

Component* Spec::get_component_for_instance(const std::string& name)
{
	Instance* inst = (Instance*)get_system()->get_object(name);
	assert(inst->get_type() == SysObject::INSTANCE);

	return get_component(inst->get_component());
}

Linkpoint* Spec::get_linkpoint(const LinkTarget& path)
{
	Component* comp = get_component_for_instance(path.get_inst());
	Interface* iface = comp->get_interface(path.get_iface());
	return iface->get_linkpoint(path.get_lp());
}

void Spec::validate()
{
	// Validate each component
	for (auto& i : s_components)
	{
		i.second->validate();			
	}

	// Validate system
	s_system.validate();
}
