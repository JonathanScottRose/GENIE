#pragma once

#include "sys_spec.h"
#include "comp_spec.h"

namespace ct
{
namespace Spec
{
	typedef std::forward_list<Component*> ComponentList;

	void define_component(Component* comp);
	Component* get_component(const std::string& name);
	Component* get_component_for_instance(const std::string& name);
	Linkpoint* get_linkpoint(const LinkTarget& path);
	
	ComponentList get_all_components();
	System* get_system();

	void validate();
}
}