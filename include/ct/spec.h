#pragma once

#include "sys_spec.h"
#include "comp_spec.h"

namespace ct
{
namespace Spec
{
	typedef std::unordered_map<std::string, Component*> Components;
	typedef std::unordered_map<std::string, System*> Systems;

	// Access components
	void define_component(Component* comp);
	Component* get_component(const std::string& name);
	
	// Access systems
	const Systems& systems();
	void define_system(System* sys);
	System* get_system(const std::string& name);

	void validate();
	void create_subsystems();
}
}