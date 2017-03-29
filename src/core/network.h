#pragma once

#include <string>
#include "prop_macros.h"
#include "genie/port.h"
#include "genie/link.h"

namespace genie
{
namespace impl
{
	class HierObject;
	class Link;
	class Endpoint;
	class EndpointPair;

	using NetType = unsigned;
	const NetType NET_INVALID = std::numeric_limits<NetType>::max();

	class Network
	{
	public:
		PROP_GET_SET(name, const std::string&, m_short_name);
		PROP_GET_SET(id, NetType, m_id);
		PROP_GET_SET(default_max_in_conns, unsigned, m_default_max_conn_in);
		PROP_GET_SET(default_max_out_conns, unsigned, m_default_max_conn_out);

		Network() = default;
		virtual ~Network() = default;

		virtual Link* create_link() const = 0;

	protected:
		unsigned m_default_max_conn_in;
		unsigned m_default_max_conn_out;
		std::string m_short_name;
		NetType m_id;
	};

	// Allows connections to be made of a certain network type
	class Endpoint
	{
	public:
		static const unsigned UNLIMITED = std::numeric_limits<unsigned>::max();

		using Objects = std::vector<HierObject*>;
		using Endpoints = std::vector<Endpoint*>;
		using Links = std::vector<Link*>;

		Endpoint(NetType type, genie::Port::Dir dir, HierObject* parent);
		Endpoint(const Endpoint&);
		~Endpoint();

		PROP_GET(type, NetType, m_type);
		PROP_GET(dir, genie::Port::Dir, m_dir);
		PROP_GET_SET(obj, HierObject*, m_obj);

		PROP_GET_SET(max_links, unsigned, m_max_links);

		void add_link(Link*);
		void remove_link(Link*);
		void remove_all_links();
		bool has_link(Link*) const;
		const Links& links() const;
		Link* get_link0() const;

		Endpoint* get_remote0() const;
		Endpoints get_remotes() const;
		HierObject* get_remote_obj0() const;
		Objects get_remote_objs() const;

		bool is_connected() const;

		Endpoint* get_sibling() const;

	protected:
		HierObject* m_obj;
		genie::Port::Dir m_dir;
		NetType m_type;
		Links m_links;
		unsigned int m_max_links;

		const Network* get_network() const;
	};

	// A connection for a particular network type.
	class Link : virtual public genie::Link
	{
	public:
		Link();
		Link(const Link&);
		Link(Endpoint* src, Endpoint* sink);
		virtual ~Link();

		Endpoint* get_src_ep() const;
		Endpoint* get_sink_ep() const;
		Endpoint* get_other_ep(const Endpoint*) const;

		HierObject* get_src() const;
		HierObject* get_sink() const;

		void set_src_ep(Endpoint*);
		void set_sink_ep(Endpoint*);

		NetType get_type() const;

		// Duplicate link.
		virtual Link* clone() const;

	protected:
		Endpoint* m_src;
		Endpoint* m_sink;
	};

	class EndpointPair
	{
	public:
		Endpoint* in;
		Endpoint* out;

		EndpointPair();
		~EndpointPair() = default;
		EndpointPair(const EndpointPair&) = default;
	};

	/*
	// An aspect that can be attached to links which says:
	// which (higher abstraction level) links does this link implement?
	// which (lower abstraciton level) links does this link contain?
	class ALinkContainment : public AspectWithRef<Link>
	{
	public:
		typedef List<Link*> Links;

		ALinkContainment();
		~ALinkContainment();

		void add_parent_link(Link* other) { add_link(other, PARENT); }
		void add_parent_links(const Links& other) { add_links(other, PARENT); }
		void remove_parent_link(Link* other) { remove_link(other, PARENT); }
		Link* get_parent_link0() const { return get_link0(PARENT); }
		const Links& get_parent_links() const { return m_links[PARENT]; }
		bool has_parent_links() const { return !m_links[PARENT].empty(); }
		Links get_all_parent_links(NetType type) const { return get_all_links(type, PARENT); }

		void add_child_link(Link* other) { add_link(other, CHILD); }
		void add_child_links(const Links& other) { add_links(other, CHILD); }
		void remove_child_link(Link* other) { remove_link(other, CHILD); }
		Link* get_child_link0() const { return get_link0(CHILD); }
		const Links& get_child_links() const { return m_links[CHILD]; }
		bool has_child_links() const { return !m_links[CHILD].empty(); }
		Links get_all_child_links(NetType type) const { return get_all_links(type, CHILD); }

	protected:
		enum PorC
		{
			PARENT = 0,
			CHILD = 1
		};

		PorC rev_rel(PorC porc) { return (PorC)(1 - porc); }

		void add_links(const Links&, PorC);
		void add_link(Link*, PorC);
		void remove_link(Link*, PorC);
		Link* get_link0(PorC) const;
		const Links& get_links(PorC porc) const { return m_links[porc]; }
		bool has_links(PorC porc) const { return !m_links[porc].empty(); }
		Links get_all_links(NetType, PorC) const;

		void remove_link_internal(Link*, PorC);
		void add_link_internal(Link*, PorC);

		Links m_links[2];
	};*/
}
}
