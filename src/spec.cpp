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

	void follow_export(Export* e, Instance** out_inst, Interface** out_iface)
	{
		System* s = e->get_parent();

		for (auto& link : s->links())
		{
			const LinkTarget* tgts[] = {&link->get_src(), &link->get_dest()};
			const LinkTarget* others[] = {&link->get_dest(), &link->get_src()};
			for (int i = 0; i < 2; i++)
			{
				auto tgt = tgts[i];
				auto other = others[i];

				if (tgt->get_inst() != e->get_name())
					continue;

				*out_inst = (Instance*)s->get_object(other->get_inst());
				assert(*out_inst && (*out_inst)->get_type() == SysObject::INSTANCE);

				Component* comp = s->get_component_for_instance((*out_inst)->get_name());
				assert(comp);
				*out_iface = comp->get_interface(other->get_iface());

				return;
			}
		}
	}

	std::string follow_clock(Instance* inst, DataInterface* src_iface)
	{
		// Clock interface name
		const std::string& src_clk_iface_name = src_iface->get_clock();

		// Instance name
		const std::string& src_inst_name = inst->get_name();

		// Find what it's connected to
		System* s = inst->get_parent();

		for (auto& link : s->links())
		{
			const LinkTarget& dest = link->get_dest();
			
			if (dest.get_inst() == src_inst_name && 
				dest.get_iface() == src_clk_iface_name)
			{
				const LinkTarget& src = link->get_src();
				return src.get_inst();
			}
		}

		assert(false);
		return "";
	}
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

void Spec::validate()
{
	// Validate each component
	for (auto& i : s_components)
	{
		i.second->validate();			
	}

	// Validate systems
	for (auto& i : s_systems)
	{
		i.second->validate();
	}
}

void Spec::create_subsystems()
{
	for (auto& i : s_systems)
	{
		System* s = i.second;

		// Create component using system's name
		Component* comp = new Component(s->get_name());

		// Convert system's exports into interfaces
		for (auto& j : s->objects())
		{
			Export* exp = (Export*)j.second;
			if (exp->get_type() != SysObject::EXPORT)
				continue;

			// Follow the export to (the, an) interface that it exports
			Interface* old_iface = nullptr;
			Instance* old_inst = nullptr;
			follow_export(exp, &old_inst, &old_iface);

			Interface* new_iface = nullptr;

			if (!old_iface && exp->get_iface_type() == Interface::RESET_SINK)
			{
				// No exported interface found because this is an auto-generated reset sink
				// and the system doesn't use resets

				// Create a bare reset interface with no signals (hacky hack)
				new_iface = new ClockResetInterface(exp->get_name(), Interface::RESET_SINK, comp);
			}
			else
			{
				// Clone the exported interface
				new_iface = old_iface->clone();
			
				// Fixup name to be the export's name
				new_iface->set_name(exp->get_name());

				// Fixup parent to be new component
				new_iface->set_parent(comp);

				// Fixup associated clock for data interfaces
				if (new_iface->get_type() == Interface::SEND ||
					new_iface->get_type() == Interface::RECV)
				{
					std::string exported_clock_iface = 
						follow_clock(old_inst, (DataInterface*)old_iface);

					((DataInterface*)new_iface)->set_clock(exported_clock_iface);
				}

				// Turn any parameterized widths into concrete const widths
				for (Signal* s : new_iface->signals())
				{
					int width = s->get_width().get_value([=](const std::string& name)
					{
						return &(old_inst->get_param_binding(name));
					});

					// Replace width expression with a ConstExpression
					s->set_width(width);
				}
			}

			// Register interface
			comp->add_interface(new_iface);
		}

		// Register new component
		define_component(comp);
	}
}

bool Spec::is_subsystem_of(System* a, System* b)
{
	// is 'a' a subsystem of 'b'?

	// Go through B's instances and find an instance whose component name is A's name
	for (auto& i : b->objects())
	{
		auto inst = (Instance*)i.second;
		if (inst->get_type() != SysObject::INSTANCE)
			continue;

		if (inst->get_component() == a->get_name())
			return true;
	}

	return false;
}

Spec::SystemOrder Spec::get_elab_order()
{
	SystemOrder result;
	
	for (auto& i : s_systems)
	{
		result.push_back(i.second);
	}

	std::sort(result.begin(), result.end(), 
		[](System* a, System* b)
		{
			return is_subsystem_of(a, b);
		}
	);

	return result;
}