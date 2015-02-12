#pragma once

#include "ct/common.h"
#include "ct/comp_spec.h"

namespace ct
{
namespace Spec
{

class SysObject;
class Instance;
class Link;
class LinkTarget;
class System;

class TopoGraph;


class LinkTarget
{
public:
	LinkTarget();
	LinkTarget(const std::string& path);

	PROP_GET_SET(inst, const std::string&, m_inst);
	PROP_GET_SET(iface, const std::string&, m_iface);
	PROP_GET_SET(lp, const std::string&, m_lp);

	std::string get_path() const;

	bool operator== (const LinkTarget& other) const;

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
	PROP_GET_SET(label, const std::string&, m_label);

protected:
	std::string m_label;
	LinkTarget m_src;
	LinkTarget m_dest;
};

typedef std::list<Link*> Links;

class SysObject
{
public:
	enum Type
	{
		INSTANCE,
		EXPORT
	};

	SysObject(const std::string& name, Type type, System* parent);
	virtual ~SysObject();

	PROP_GET_SET(name, const std::string&, m_name);
	PROP_GET_SET(type, Type, m_type);
	PROP_GET_SET(parent, System*, m_parent);

protected:
	System* m_parent;
	std::string m_name;
	Type m_type;
};


class Instance : public SysObject
{
public:
	typedef std::unordered_map<std::string, Expression> ParamBindings;

	Instance(const std::string& name, const std::string& component, System* parent);
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
	Export(const std::string& name, Interface* iface, System* parent);
	~Export();

	PROP_GET_SET(iface, Interface*, m_iface);
	
protected:
	Interface* m_iface;
};


struct LatencyQuery
{
	std::string link_label;
	std::string param_name;
};


class System
{
public:
	typedef std::unordered_map<std::string, SysObject*> Objects;
	typedef std::unordered_map<std::string, Link*> LinksByLabel;
	typedef std::vector<std::string> ExclusionGroup;
	typedef std::vector<ExclusionGroup> ExclusionGroups;
	typedef std::vector<LatencyQuery> LatencyQueries;

	System();
	~System();

	PROP_GET_SET(name, const std::string&, m_name);
	PROP_GET(topology, TopoGraph*, m_topo);

	const Links& links() { return m_links; }
	void add_link(Link* link);
	Link* get_link(const std::string& label) const;
	Link* get_link(const LinkTarget&, const LinkTarget&) const;

	const ExclusionGroups& exclusion_groups() const;
	void add_exclusion_group(const ExclusionGroup&);
	ExclusionGroups exclusion_groups_for_link(Link* link) const;

	const LatencyQueries& latency_queries() const;
	void add_latency_query(const LatencyQuery&);

	const Objects& objects() { return m_objects; }
	void add_object(SysObject* inst);
	SysObject* get_object(const std::string& name);

	Component* get_component_for_instance(const std::string& name);
	Linkpoint* get_linkpoint(const LinkTarget& path);

	const Instance::ParamBindings& param_bindings() { return m_param_bindings; }
	const Expression& get_param_binding(const std::string& name);
	void set_param_binding(const std::string& name, const Expression& expr);

protected:
	std::string m_name;
	Links m_links;
	LinksByLabel m_links_by_label;
	ExclusionGroups m_exclusion_groups;
	LatencyQueries m_latency_queries;
	Objects m_objects;
	TopoGraph* m_topo;
	Instance::ParamBindings m_param_bindings;
};


}
}
