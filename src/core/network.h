#pragma once

#include <string>
#include "prop_macros.h"
#include "graph.h"
#include "genie_priv.h"
#include "genie/port.h"
#include "genie/link.h"

namespace genie
{
namespace impl
{
	class Node;
	class NodeSystem;
	class HierObject;
	class Link;
	class Endpoint;
	class EndpointPair;

	class NetworkDef
	{
	public:
		PROP_GET_SET(name, const std::string&, m_short_name);
		PROP_GET_SET(id, NetType, m_id);
		PROP_GET_SET(default_max_in_conns, unsigned, m_default_max_conn_in);
		PROP_GET_SET(default_max_out_conns, unsigned, m_default_max_conn_out);

		NetworkDef() = default;
		virtual ~NetworkDef() = default;

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

		const NetworkDef* get_network() const;
	};

	// An integer ID for a link.
	// Encodes its type and index of that type
	struct LinkID
	{
	private:
		uint32_t _id;

	public:
		constexpr LinkID(NetType type, uint16_t index)
			: _id((type << 16U) | index) {}
		LinkID() = default;

		void set_index(uint16_t idx);
		uint16_t get_index() const;

		void set_type(NetType type);
		NetType get_type() const;

		bool operator< (const LinkID& o) const;
		bool operator== (const LinkID& o) const;
		bool operator!= (const LinkID& o) const;

		explicit LinkID(uint32_t val);
		explicit operator uint32_t() const;
	};

	constexpr LinkID LINK_INVALID = { NET_INVALID, std::numeric_limits<uint16_t>::max() };

	// A connection for a particular network type.
	class Link : virtual public genie::Link
	{
	public:
		Link();
		Link(const Link&);
		Link(Endpoint* src, Endpoint* sink);
		virtual ~Link();

		PROP_GET_SET(id, LinkID, m_id);

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
		LinkID m_id;
		Endpoint* m_src;
		Endpoint* m_sink;
	};

	class LinksContainer
	{
	public:
		LinksContainer();

		PROP_GET_SET(type, NetType, m_type);
		LinkID insert(Link*);
		Link* get(LinkID);
		Link* remove(LinkID);
		const std::vector<Link*>& get_all() const;

	protected:
		uint16_t find_pos(LinkID);

		NetType m_type;
		std::vector<Link*> m_links;
		uint16_t m_next_index;
		bool m_compact;
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

	class LinkRelations
	{
	public:
		LinkRelations* clone(const Node * srcsys, Node * destsys) const;
		void reintegrate(LinkRelations* src, const Node* srcsys,
			const Node* destsys);

		void add(Link* parent, Link* child);
		void remove(Link* parent, Link* child);
		bool is_contained_in(Link* parent, Link* child) const;
		void unregister_link(Link* link);

		std::vector<Link*> get_immediate_parents(Link* link) const;
		std::vector<Link*> get_immediate_children(Link* link) const;

		template<class T = Link>
		std::vector<T*> get_parents(Link* link, NetType net) const
		{
			std::vector<T*> result;
			get_porc_internal(link, net, &graph::Graph::dir_neigh_r, &result,
				[](Link* p) { return static_cast<T*>(p); });
			return result;
		}

		template<class T = Link>
		std::vector<T*> get_children(Link* link, NetType net) const
		{
			std::vector<T*> result;
			get_porc_internal(link, net, &graph::Graph::dir_neigh, &result,
				[](Link* p) { return static_cast<T*>(p); });
			return result;
		}

	protected:
		using ThuncFunc = std::function<void*(Link*)>;

		void get_porc_internal(Link* link, NetType net,
			graph::VList(graph::Graph::*)(graph::VertexID) const,
			void* out, const ThuncFunc&) const;

		std::vector<Link*> get_immediate_porc_internal(Link*,
			graph::VList(graph::Graph::*)(graph::VertexID) const) const;

		graph::V2Attr<Link*> m_v2link;
		graph::Attr2V<Link*> m_link2v;
		graph::Graph m_graph;
	};
}
}

// Hash function for LinkID
namespace std
{
	template<> struct hash<genie::impl::LinkID>
	{
		using argument_type = genie::impl::LinkID;
		using result_type = size_t;
		
		result_type operator()(const argument_type& obj) const
		{
			return std::hash<uint32_t>{}((uint32_t)obj);
		}
	};
}