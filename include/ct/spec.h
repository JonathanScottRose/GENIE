#pragma once

#include "sys_spec.h"
#include "comp_spec.h"
#include "topo_spec.h"

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

	// Elaboration
	typedef std::vector<System*> SystemOrder;

	void validate();
	void create_subsystems();
	bool is_subsystem_of(System* a, System* b);
	SystemOrder get_elab_order();
}
}