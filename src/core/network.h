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

		void disconnect_src();
		void disconnect_sink();

		void reconnect_src(Endpoint*);
		void reconnect_sink(Endpoint*);

		void set_src_ep(Endpoint*);
		void set_sink_ep(Endpoint*);

		NetType get_type() const;

		virtual Link* clone() const;
		//virtual bool is_duplicate(Link* other) const;

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
		LinkID insert_new(Link*);
		void insert_existing(Link*);
		std::vector<Link*> move_new_from(LinksContainer&);
		void prepare_for_copy(const LinksContainer&);
		Link* get(LinkID);
		Link* remove(LinkID);
		std::vector<Link*> get_all() const;

		template<class T=Link>
		std::vector<T*> get_all_casted() const
		{
			std::vector<T*> result;
			get_all_internal(&result, [](Link* p) { return static_cast<T*>(p); });
			return result;
		}

	protected:
		using ThuncFunc = std::function<void*(Link*)>;

		void get_all_internal(void* out, const ThuncFunc&) const;
		void ensure_size_for_index(uint16_t idx);

		NetType m_type;
		uint16_t m_next_id;
		std::vector<Link*> m_links;
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
		void prune(Node* dest);
		void reintegrate(LinkRelations& src);

		void add(LinkID parent, LinkID child);
		void remove(LinkID parent, LinkID child);
		bool is_contained_in(LinkID parent, LinkID child) const;
		void unregister_link(LinkID link);

		std::vector<LinkID> get_immediate_parents(LinkID link) const;
		std::vector<LinkID> get_immediate_children(LinkID link) const;
		std::vector<LinkID> get_parents(LinkID link, NetType type) const;
		std::vector<LinkID> get_children(LinkID link, NetType type) const;

		template<class T = Link>
		std::vector<T*> get_parents(Link* link, NetType net, Node* sys) const
		{
			std::vector<T*> result;
			get_porc_internal(link, net, sys, &graph::Graph::dir_neigh_r, &result,
				[](Link* p) { return static_cast<T*>(p); });
			return result;
		}

		template<class T = Link>
		std::vector<T*> get_children(Link* link, NetType net, Node* sys) const
		{
			std::vector<T*> result;
			get_porc_internal(link, net,sys,  &graph::Graph::dir_neigh, &result,
				[](Link* p) { return static_cast<T*>(p); });
			return result;
		}

	protected:
		using ThuncFunc = std::function<void*(Link*)>;

		void get_porc_internal(Link* link, NetType net, Node* sys,
			graph::VList(graph::Graph::*)(graph::VertexID) const,
			void* out, const ThuncFunc&) const;

		std::vector<LinkID> get_immediate_porc_internal(LinkID link,
			graph::VList(graph::Graph::*)(graph::VertexID) const) const;

		std::vector<LinkID> get_porc_internal(LinkID link, NetType net,
			graph::VList(graph::Graph::*)(graph::VertexID) const) const;

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