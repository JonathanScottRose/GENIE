#pragma once

#include "ct/sys_spec.h"
#include "ct/comp_spec.h"
#include "ct/topo_spec.h"

namespace ct
{
namespace Spec
{
	typedef std::unordered_map<std::string, Component*> Components;
	typedef std::unordered_map<std::string, System*> Systems;

	// Access components
	const Components& components();
	void define_component(Component* comp);
	Component* get_component(const std::string& name);
	
	// Access systems
	const Systems& systems();
	void define_system(System* sys);
	System* get_system(const std::string& name);
}
}