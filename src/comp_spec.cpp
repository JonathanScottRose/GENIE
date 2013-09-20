#include "comp_spec.h"

using namespace ct;
using namespace ct::Spec;

//
// Signal
//

Signal::Signal(Type type, const Expression& width)
	: m_width(width), m_type(type), m_impl(nullptr)
{
	if (type == CLOCK || type == RESET || type == VALID || type == READY ||
		type == SOP || type == EOP)
	{
		assert(width.get_const_value() == 1);
	}
}

Signal::~Signal()
{
	if (m_impl) delete m_impl;
}

Signal::Type Signal::type_from_string(const std::string& str)
{
	std::string str2 = Util::str_tolower(str);

	if (str2 == "clock") return CLOCK;
	else if (str2 == "reset") return RESET;
	else if (str2 == "data") return DATA;
	else if (str2 == "header") return HEADER;
	else if (str2 == "valid") return VALID;
	else if (str2 == "ready") return READY;
	else if (str2 == "sop") return SOP;
	else if (str2 == "eop") return EOP;
	else if (str2 == "lp_id") return LP_ID;
	else if (str2 == "link_id") return LINK_ID;
	else throw std::exception(("Unknown signal type " + str).c_str());

	return CLOCK;
}

//
// Linkpoint
//

Linkpoint::Linkpoint(const std::string& name, Type type, DataInterface* iface)
	: m_name(name), m_type(type), m_encoding(nullptr), m_iface(iface)
{
}

Linkpoint::~Linkpoint()
{
	if (m_encoding) delete m_encoding;
}

//
// Interface
//

Interface::Interface(const std::string& name, Type type)
	: m_name(name), m_type(type)
{
}

Interface::~Interface()
{
	Util::delete_all(m_signals);
	Util::delete_all_2(m_linkpoints);
}

void Interface::add_signal(Signal* sig)
{
	m_signals.push_front(sig);
}

Interface::Signals Interface::get_signals(Signal::Type type)
{
	Signals ret;
	for (auto& i : m_signals)
	{
		if (i->get_type() == type)
			ret.push_front(i);
	}

	return ret;
}

Signal* Interface::get_signal(Signal::Type type)
{
	Signals ret = get_signals(type);
	return ret.empty() ? nullptr : ret.front();
}

Signal* Interface::get_signal(Signal::Type type, const std::string& subtype)
{
	for (auto& i : m_signals)
	{
		if (i->get_type() == type && i->get_subtype() == subtype)
			return i;
	}

	return nullptr;
}

Interface::Type Interface::type_from_string(const std::string& str)
{
	std::string str2 = Util::str_tolower(str);
	if (str2 == "clock_sink") return Type::CLOCK_SINK;
	else if (str2 == "clock_src") return Type::CLOCK_SRC;
	else if (str2 == "reset_src") return Type::RESET_SRC;
	else if (str2 == "reset_sink") return Type::RESET_SINK;
	else if (str2 == "send") return Type::SEND;
	else if (str2 == "recv") return Type::RECV;
	else throw std::exception(("Unknown interface type: " + str).c_str());
	
	return Type::CLOCK_SRC;
}

Interface* Interface::factory(const std::string& name, Type type)
{
	switch(type)
	{
	case SEND:
	case RECV:
		return new DataInterface(name, type);
	case CLOCK_SRC:
	case CLOCK_SINK:
	case RESET_SRC:
	case RESET_SINK:
		return new ClockResetInterface(name, type);
	}

	return nullptr;
}

void Interface::add_linkpoint(Linkpoint* lp)
{
	assert(m_linkpoints.count(lp->get_name()) == 0);
	m_linkpoints[lp->get_name()] = lp;
}

Linkpoint* Interface::get_linkpoint(const std::string& name)
{
	return m_linkpoints[name];
}

//
// ClockResetInterface
//

ClockResetInterface::ClockResetInterface(const std::string& name, Type type)
	: Interface(name, type)
{
}

ClockResetInterface::~ClockResetInterface()
{
}

//
// DataInterface
//

DataInterface::DataInterface(const std::string& name, Type type)
	: Interface(name, type)
{
}

DataInterface::~DataInterface()
{
	Util::delete_all_2(m_linkpoints);
}

//
// Component
//

Component::Component(const std::string& name)
	: m_name(name), m_impl(nullptr)
{
}

Component::~Component()
{
	Util::delete_all_2(m_interfaces);
	if (m_impl) delete m_impl;
}

void Component::add_interface(Interface* iface)
{
	assert(!Util::exists_2(m_interfaces, iface->get_name()));
	m_interfaces[iface->get_name()] = iface;
}

Component::InterfaceList Component::get_interfaces(Interface::Type type)
{
	InterfaceList ret;
	for (auto& i : m_interfaces)
	{
		if (i.second->get_type() == type)
			ret.push_front(i.second);
	}
	return ret;
}

Interface* Component::get_interface(const std::string& name)
{
	return m_interfaces[name];
}

void Component::validate()
{
	for (auto& i : interfaces())
	{
		Interface* iface = i.second;

		if (iface->get_type() == Interface::SEND ||
			iface->get_type() == Interface::RECV)
		{
			if (iface->linkpoints().empty())
			{
				Linkpoint* lp = new Linkpoint("lp", Spec::Linkpoint::UNICAST, (DataInterface*)iface);
				iface->add_linkpoint(lp);
			}
		}
	}
}

