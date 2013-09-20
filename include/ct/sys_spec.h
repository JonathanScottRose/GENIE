#pragma once

#include "common.h"
#include "comp_spec.h"

namespace ct
{
namespace Spec
{

class SysObject;
class Instance;
class Link;
class LinkTarget;
class InterfaceState;
class System;


class LinkTarget
{
public:
	LinkTarget();
	LinkTarget(const std::string& path);

	PROP_GET_SET(inst, const std::string&, m_inst);
	PROP_GET_SET(iface, const std::string&, m_iface);
	PROP_GET_SET(lp, const std::string&, m_lp);

	std::string get_path() const;

private:
	std::string m_inst;
	std::string m_iface;
	std::string m_lp;
};


class Link
{
public:
	Link(const LinkTarget& src, const LinkTarget& dest);
	~Link();

	PROP_GET_SET(src, const LinkTarget&, m_src);
	PROP_GET_SET(dest, const LinkTarget&, m_dest);

protected:
	LinkTarget m_src;
	LinkTarget m_dest;
};


class LinkBinding
{
public:

protected:
};


class InterfaceState
{
public:
	InterfaceState(const std::string& name);

protected:
	std::string m_name;
};


class SysObject
{
public:
	enum Type
	{
		INSTANCE,
		EXPORT
	};

	SysObject(const std::string& name, Type type);
	virtual ~SysObject();

	PROP_GET_SET(name, const std::string&, m_name);
	PROP_GET_SET(type, Type, m_type);

protected:
	std::string m_name;
	Type m_type;
};


class Instance : public SysObject
{
public:
	typedef std::unordered_map<std::string, Expression> ParamBindings;

	Instance(const std::string& name, const std::string& component);
	~Instance();

	PROP_GET_SET(component, const std::string&, m_component);

	const ParamBindings& param_bindings() { return m_param_bindings; }
	const Expression& get_param_binding(const std::string& name);
	void set_param_binding(const std::string& name, const Expression& expr);

protected:
	ParamBindings m_param_bindings;
	std::string m_component;
};


class Export : public SysObject
{
public:
	Export(const std::string& name, Interface::Type type);

	PROP_GET_SET(iface_type, Interface::Type, m_iface_type);

protected:
	Interface::Type m_iface_type;
};


class System
{
public:
	typedef std::list<Link*> Links;
	typedef std::unordered_map<std::string, SysObject*> Objects;

	System();
	~System();

	PROP_GET_SET(name, const std::string&, m_name);

	const Links& links() { return m_links; }
	void add_link(Link* link);

	const Objects& objects() { return m_objects; }
	void add_object(SysObject* inst);
	SysObject* get_object(const std::string& name);

	void validate();

protected:
	std::string m_name;
	Links m_links;
	Objects m_objects;
};


}
}