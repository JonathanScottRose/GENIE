#include <sstream>
#include <unordered_set>
#include "spec.h"

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

Export::Export(const std::string& name, Interface::Type iface_type, System* parent)
	: SysObject(name, EXPORT, parent), m_iface_type(iface_type)
{
}

//
// System
//

System::System()
{
}

System::~System()
{
	Util::delete_all(m_links);
	Util::delete_all_2(m_objects);
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

void System::validate()
{
	// Auto-export unexported interfaces
	std::unordered_set<std::string> srcs, dests;
	for (auto& i : m_links)
	{
		srcs.insert(i->get_src().get_path());
		dests.insert(i->get_dest().get_path());
	}

	for (auto& i : m_objects)
	{
		Instance* inst = (Instance*)i.second;
		if (inst->get_type() != SysObject::INSTANCE)
			continue;

		const std::string& instname = inst->get_name();

		LinkTarget tmp;
		tmp.set_inst(instname);

		Component* comp = get_component_for_instance(instname);

		for (auto& j : comp->interfaces())
		{
			const std::string& ifname = j.first;
			Interface* iface = j.second;
			
			tmp.set_iface(ifname);

			bool is_out;
			bool is_data;

			switch (iface->get_type())
			{
				case Interface::SEND:
				case Interface::RECV:
					is_data = true;
					break;
				default:
					is_data = false;
					break;
			}

			switch (iface->get_type())
			{
				case Interface::SEND:
				case Interface::CLOCK_SRC:
				case Interface::RESET_SRC:
				case Interface::CONDUIT:
					is_out = true;
					break;
				default:
					is_out = false;
					break;
			}

			for (auto& k : iface->linkpoints())
			{
				tmp.set_lp(k.first);
				bool connected;

				if (is_out)	connected = srcs.count(tmp.get_path()) > 0;
				else connected = dests.count(tmp.get_path()) > 0;

				if (!connected)
				{
					assert(! (is_data && iface->linkpoints().size() > 1) );

					std::string exname = instname + '_' + ifname;
					Export* ex = new Export(exname, iface->get_type(), this);
					add_object(ex);

					LinkTarget ex_target(exname);
					Link* link;
					if (is_out) link = new Link(tmp, ex_target);
					else link = new Link(ex_target, tmp);

					add_link(link);
				}
			}
		}
	}

	// Create reset export if there are none
	bool has_reset = false;
	for (auto& i : m_objects)
	{
		Export* obj = (Export*)i.second;

		if (obj->get_type() != SysObject::EXPORT)
			continue;

		if (obj->get_iface_type() == Interface::RESET_SINK)
		{
			has_reset = true;
			break;
		}
	}

	if (!has_reset)
	{
		Export* ex = new Export("reset", Interface::RESET_SINK, this);
		add_object(ex);
	}
}