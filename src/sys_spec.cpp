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
		m_iface = "";
		m_lp = "";
	}
	else if (!std::getline(strm, m_lp, '.'))
	{
		m_lp = "lp";
	}
}

std::string LinkTarget::get_path() const
{
	std::string result = m_inst;
	if (!m_iface.empty())
	{
		result += '.' + m_iface;
		if (!m_lp.empty()) result += '.' + m_lp;
		else result += ".lp";
	}	

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

SysObject::SysObject(const std::string& name, Type type)
	: m_name(name), m_type(type)
{
}

SysObject::~SysObject()
{
}

//
// Instance
//

Instance::Instance(const std::string& name, const std::string& component)
	: SysObject(name, INSTANCE), m_component(component)
{
}

Instance::~Instance()
{
}

//
// Export
//

Export::Export(const std::string& name, Interface::Type iface_type)
	: SysObject(name, EXPORT), m_iface_type(iface_type)
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

		Component* comp = Spec::get_component_for_instance(instname);

		for (auto& j : comp->interfaces())
		{
			const std::string& ifname = j.first;
			Interface* iface = j.second;
			
			tmp.set_iface(ifname);

			bool is_out;
			bool is_data = false;

			switch (iface->get_type())
			{
				case Interface::RECV:
					is_data = true;
				case Interface::CLOCK_SINK:
				case Interface::RESET_SINK:
					is_out = false;
					break;
				default:
					is_out = true;
					break;
			}

			bool connected;

			if (is_out)	connected = srcs.count(tmp.get_path()) > 0;
			else connected = dests.count(tmp.get_path()) > 0;

			if (!connected)
			{
				assert(! (is_data && iface->linkpoints().size() > 1) );

				std::string exname = instname + '_' + ifname;
				Export* ex = new Export(exname, iface->get_type());
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