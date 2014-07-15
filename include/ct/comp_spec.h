#pragma once

#include "ct/common.h"
#include "ct/expressions.h"

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

class Signal : public HasImplAspect
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
		LINK_ID,
		CONDUIT_IN,
		CONDUIT_OUT
	};

	enum Sense
	{
		FWD,
		REV
	};
	
	Signal(Type type, const Expression& width);
	~Signal();

	PROP_GET_SET(width, const Expression&, m_width);
	PROP_GET_SET(type, Type, m_type);
	PROP_GET_SET(subtype, const std::string&, m_subtype);
	PROP_GET(sense, Sense, m_sense);

	std::string get_field_name() const;

	static Type type_from_string(const std::string& str);

protected:
	Type m_type;
	Sense m_sense;
	std::string m_subtype;	
	Expression m_width;
};

class Linkpoint : public HasImplAspect
{
public:
	enum Type
	{
		UNICAST,
		BROADCAST
	};

	Linkpoint(const std::string& name, Type type, Interface* iface);
	~Linkpoint();

	PROP_GET_SET(name, const std::string&, m_name);
	PROP_GET_SET(iface, Interface*, m_iface);
	PROP_GET(type, Type, m_type);

	static Type type_from_string(const std::string& type);

protected:	
	std::string m_name;
	Interface* m_iface;
	Type m_type;
};

class Interface
{
public:
	typedef std::list<Signal*> Signals;
	typedef std::unordered_map<std::string, Linkpoint*> Linkpoints;

	enum Type
	{
		CLOCK,
		RESET,
		DATA,
		CONDUIT
	};

	enum Dir
	{
		IN,
		OUT
	};

	Interface(const std::string& name, Type type, Dir dir);
	Interface(const Interface&);
	virtual ~Interface();
	virtual Interface* clone() = 0;

	PROP_GET_SET(name, const std::string&, m_name);
	PROP_GET_SET(type, Type, m_type);
	PROP_GET_SET(dir, Dir, m_dir);

	const Signals& signals() { return m_signals; }
	void add_signal(Signal* sig);
	Signals get_signals(Signal::Type type);
	Signal* get_signal(Signal::Type type);
	Signal* get_signal(Signal::Type type, const std::string& subtype);
	Signal* get_signal(const std::string& field_name);

	const Linkpoints& linkpoints() { return m_linkpoints; }
	void add_linkpoint(Linkpoint* lp);
	Linkpoint* get_linkpoint(const std::string& name);

	static Type type_from_string(const std::string& str);
	static Dir dir_from_string(const std::string& str);
	static Interface* factory(const std::string& name, Type type, Dir dir);

protected:
	std::string m_name;
	Type m_type;
	Dir m_dir;
	Signals m_signals;
	Linkpoints m_linkpoints;
};

class ClockResetInterface : public Interface
{
public:
	ClockResetInterface(const std::string& name, Type type, Dir dir);
	~ClockResetInterface();
	Interface* clone();
};

class ConduitInterface : public Interface
{
public:
	ConduitInterface(const std::string& name, Dir dir);
	~ConduitInterface();
	Interface* clone();
};

class DataInterface : public Interface
{
public:
	DataInterface(const std::string& name, Type type, Dir dir);
	~DataInterface();
	Interface* clone();

	PROP_GET_SET(clock, const std::string&, m_clock);

protected:
	std::string m_clock;
};

class Component : public HasImplAspect
{
public:
	typedef std::unordered_map<std::string, Interface*> Interfaces;
	typedef std::forward_list<Interface*> InterfaceList;
	typedef std::vector<std::string> Parameters;

	Component(const std::string& name);
	~Component();

	PROP_GET_SET(name, const std::string&, m_name);

	const Interfaces& interfaces() { return m_interfaces; }
	void add_interface(Interface* iface);
	InterfaceList get_interfaces(Interface::Type type);
	Interface* get_interface(const std::string& name);

	void add_parameter(const std::string& p) { m_params.push_back(p); }
	const Parameters& parameters() { return m_params; }

protected:
	std::string m_name;
	Interfaces m_interfaces;
	Parameters m_params;
};


}
}