#pragma once

#include "common.h"
#include "expressions.h"

using ct::Expressions::Expression;

namespace ct
{
namespace Spec
{

class Interface;
class ClockResetInterface;
class DataInterface;
class Linkpoint;
class Component;
class Signal;

class Signal
{
public:
	enum Type
	{
		CLOCK,
		RESET,
		DATA,
		HEADER,
		VALID,
		READY,
		SOP,
		EOP,
		LP_ID,
		LINK_ID
	};
	
	Signal(Type type, const Expression& width);
	~Signal();

	PROP_GET_SET(impl, OpaqueDeletable*, m_impl);
	PROP_GET_SET(width, const Expression&, m_width);
	PROP_GET_SET(type, Type, m_type);
	PROP_GET_SET(subtype, const std::string&, m_subtype);

	static Type type_from_string(const std::string& str);

protected:
	Type m_type;
	std::string m_subtype;
	OpaqueDeletable* m_impl;
	Expression m_width;
};

class Linkpoint
{
public:
	enum Type
	{
		UNICAST,
		BROADCAST
	};

	Linkpoint(const std::string& name, Type type, DataInterface* iface);
	~Linkpoint();

	PROP_GET_SET(name, const std::string&, m_name);
	PROP_GET_SET(encoding, OpaqueDeletable*, m_encoding);
	PROP_GET(iface, DataInterface*, m_iface);
	PROP_GET(type, Type, m_type);

protected:	
	std::string m_name;
	DataInterface* m_iface;
	Type m_type;
	OpaqueDeletable* m_encoding;
};

class Interface
{
public:
	typedef std::forward_list<Signal*> Signals;
	typedef std::unordered_map<std::string, Linkpoint*> Linkpoints;

	enum Type
	{
		CLOCK_SRC,
		CLOCK_SINK,
		RESET_SRC,
		RESET_SINK,
		SEND,
		RECV
	};

	Interface(const std::string& name, Type type);
	~Interface();

	PROP_GET_SET(name, const std::string&, m_name);
	PROP_GET_SET(type, Type, m_type);

	const Signals& signals() { return m_signals; }
	void add_signal(Signal* sig);
	Signals get_signals(Signal::Type type);
	Signal* get_signal(Signal::Type type);
	Signal* get_signal(Signal::Type type, const std::string& subtype);

	const Linkpoints& linkpoints() { return m_linkpoints; }
	void add_linkpoint(Linkpoint* lp);
	Linkpoint* get_linkpoint(const std::string& name);

	static Type type_from_string(const std::string& str);
	static Interface* factory(const std::string& name, Type type);

protected:
	std::string m_name;
	Type m_type;
	Signals m_signals;
	Linkpoints m_linkpoints;
};

class ClockResetInterface : public Interface
{
public:
	ClockResetInterface(const std::string& name, Type type);
	~ClockResetInterface();
};

class DataInterface : public Interface
{
public:
	typedef std::unordered_map<std::string, Linkpoint*> Linkpoints;

	DataInterface(const std::string& name, Type type);
	~DataInterface();

	PROP_GET_SET(clock, const std::string&, m_clock);

	const Linkpoints& linkpoints() { return m_linkpoints; }
	void add_linkpoint(Linkpoint* lp);
	Linkpoint* get_linkpoint(const std::string& name);

protected:
	Linkpoints m_linkpoints;
	std::string m_clock;
};

class Component
{
public:
	typedef std::unordered_map<std::string, Interface*> Interfaces;
	typedef std::forward_list<Interface*> InterfaceList;

	Component(const std::string& name);
	~Component();

	PROP_GET_SET(name, const std::string&, m_name);
	PROP_GET_SET(impl, OpaqueDeletable*, m_impl);

	const Interfaces& interfaces() { return m_interfaces; }
	void add_interface(Interface* iface);
	InterfaceList get_interfaces(Interface::Type type);
	Interface* get_interface(const std::string& name);

	void validate();

protected:
	std::string m_name;
	OpaqueDeletable* m_impl;
	Interfaces m_interfaces;
};


}
}