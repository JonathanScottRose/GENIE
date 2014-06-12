#include "ct/comp_spec.h"

using namespace ct;
using namespace ct::Spec;

//
// Signal
//

Signal::Signal(Type type, const Expression& width)
	: m_width(width), m_type(type)
{
	if (type == CLOCK || type == RESET || type == VALID || type == READY ||
		type == SOP || type == EOP)
	{
		assert(width.get_value() == 1);
	}

	switch (m_type)
	{
	case READY: m_sense = REV; break;
	case CONDUIT_IN: m_sense = REV; break;
	case CONDUIT_OUT: m_sense = FWD; break;
	default: m_sense = FWD; break;
	}
}

Signal::~Signal()
{
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
	else if (str2 == "in") return CONDUIT_IN;
	else if (str2 == "out") return CONDUIT_OUT;
	else throw Exception("Unknown signal type " + str);

	return CLOCK;
}

std::string Signal::get_field_name() const
{
	switch (m_type)
	{
	case CLOCK: return "clock";
	case RESET: return "reset";
	case DATA:
		{
			std::string nm = "data";
			if (!get_subtype().empty())
				nm += "_" + get_subtype();
			return nm;
		}
	case HEADER:
		{
			std::string nm = "header";
			if (!get_subtype().empty())
				nm += "_" + get_subtype();
			return nm;
		}
	case SOP: return "sop";
	case EOP: return "eop";
	case LINK_ID: return "link_id";
	case LP_ID: return "lp_id";
	case VALID: return "valid";
	case READY: return "ready";
	case CONDUIT_IN:
	case CONDUIT_OUT:
		{
			assert (!get_subtype().empty());
			return get_subtype();
		}
	default: assert (false);
	}

	return "";
}

//
// Linkpoint
//

Linkpoint::Linkpoint(const std::string& name, Type type, Interface* iface)
	: m_name(name), m_type(type), m_iface(iface)
{
}

Linkpoint::~Linkpoint()
{
}

Linkpoint::Type Linkpoint::type_from_string(const std::string& str)
{
	std::string str2 = Util::str_tolower(str);
	if (str2 == "unicast") return UNICAST;
	else if (str2 == "broadcast") return BROADCAST;
	else throw Exception("Unknown linkpoint type " + str);

	return UNICAST;
}

//
// Interface
//

Interface::Interface(const std::string& name, Type type, Dir dir, Component* parent)
	: m_name(name), m_type(type), m_parent(parent), m_dir(dir)
{
}

Interface::Interface(const Interface& other)
{
	m_name = other.m_name;
	m_type = other.m_type;
	Util::copy_all(other.m_signals, m_signals);
	Util::copy_all_2(other.m_linkpoints, m_linkpoints);

	for (auto& i : m_linkpoints)
	{
		Linkpoint* lp = i.second;
		lp->set_iface(this);
	}
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

Signal* Interface::get_signal(const std::string& field_name)
{
	for (auto& i : m_signals)
	{
		if (i->get_field_name() == field_name)
			return i;
	}

	return nullptr;
}

Interface::Type Interface::type_from_string(const std::string& str)
{
	std::string str2 = Util::str_tolower(str);

	if (str2 == "clock") return Type::CLOCK;
	else if (str2 == "reset") return Type::RESET;
	else if (str2 == "data") return Type::DATA;
	else if (str2 == "conduit") return Type::CONDUIT;
	else throw Exception("Unknown interface type: " + str);

	return Type::CLOCK;
}

Interface::Dir Interface::dir_from_string(const std::string& str)
{
	std::string str2 = Util::str_tolower(str);

	if (str2 == "in") return Dir::IN;
	else if (str2 == "out") return Dir::OUT;
	else throw Exception("Unknown interface dir: " + str);

	return Dir::OUT;
}

Interface* Interface::factory(const std::string& name, Type type, Dir dir, Component* parent)
{
	switch(type)
	{
	case CONDUIT: return new ConduitInterface(name, dir, parent);
	case DATA: return new DataInterface(name, type, dir, parent);
	case CLOCK:
	case RESET:
		return new ClockResetInterface(name, type, dir, parent);
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

ClockResetInterface::ClockResetInterface(const std::string& name, Type type, Dir dir, Component* parent)
	: Interface(name, type, dir, parent)
{
}

ClockResetInterface::~ClockResetInterface()
{
}

Interface* ClockResetInterface::clone()
{
	return new ClockResetInterface(*this);
}

//
// ConduitInterface
//

ConduitInterface::ConduitInterface(const std::string& name, Dir dir, Component* parent)
	: Interface(name, CONDUIT, dir, parent)
{
}

ConduitInterface::~ConduitInterface()
{
}

Interface* ConduitInterface::clone()
{
	return new ConduitInterface(*this);
}

//
// DataInterface
//

DataInterface::DataInterface(const std::string& name, Type type, Dir dir, Component* parent)
	: Interface(name, type, dir, parent)
{
}

DataInterface::~DataInterface()
{
}

Interface* DataInterface::clone()
{
	return new DataInterface(*this);
}

//
// Component
//

Component::Component(const std::string& name)
	: m_name(name)
{
}

Component::~Component()
{
	Util::delete_all_2(m_interfaces);
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
