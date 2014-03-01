#include "spec.h"
#include "common.h"
#include "static_init.h"

using namespace ct;
using namespace Spec;

namespace
{
	Components s_components;
	Systems s_systems;

	Util::InitShutdown s_ctor_dtor([]
	{
	}, []
	{
		Util::delete_all_2(s_components);
		Util::delete_all_2(s_systems);
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

const Systems& Spec::systems()
{
	return s_systems;
}

const Components& Spec::components()
{
	return s_components;
}

void Spec::define_system(System* sys)
{
	assert(s_systems.count(sys->get_name()) == 0);
	s_systems[sys->get_name()] = sys;
}

System* Spec::get_system(const std::string& name)
{
	if (s_systems.count(name) == 0) return nullptr;
	else return s_systems[name];
}
