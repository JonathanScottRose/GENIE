#pragma once

#include "ct/common.h"
#include "ct/networks.h"

namespace ct
{
	class Connectable;
	class Endpoint;
	class Conn;

	enum class Dir
	{
		IN,
		OUT
	};

	//
	// Interface inherited by things that can participate in connections.
	// A Connectable can participate in one ore more networks, and each specific connection point for a
	// particular network is called an Endpoint.
	// A Connectable must support at least one endpoint:
	// - if supporting just one network/endpoint, override get_default_ep()
	// - if supporting multiple networks/endpoints, override get_ep() and optionally get_default_ep()
	//   if that makes sense
	class Connectable
	{
	public:
		Dir conn_get_dir() const;
		virtual Endpoint* conn_get_ep(NetworkID) const { return conn_get_default_ep(); }
		virtual Endpoint* conn_get_default_ep() const { throw Exception("No default endpoint defined"); }
	};

	// Forward declaration of list types
	typedef std::vector<class Endpoint*> Endpoints;
	typedef std::vector<class Conn*> Conns;

	// A connection point for a particular network type
	class Endpoint
	{
	public:
		Endpoint(Connectable*);
		virtual ~Endpoint();
		virtual const NetworkID& ep_get_net_type() const = 0;
		
		Dir ep_get_dir() const;

		void ep_connect(Conn*);
		void ep_disconnect(Conn*);
		void ep_disconnect();
		const Conns& ep_conns() const;
		Conn* ep_conn0() const;
		Endpoints ep_get_remotes() const;
		Endpoint* ep_get_remote0() const;
		bool ep_has_conn(Conn*) const;
		bool ep_is_connected() const;
	
	protected:
		Conns m_conns;

	private:
		Connectable* m_parent;
		NetworkDef* _get_net_def() const;
	};

	// A connection for a particular network type.
	class Conn : public Object
	{
	public:
		Conn(const NetworkID&);
		Conn(const NetworkID&, Endpoint* src, Endpoint* sink);
		
		Endpoint* get_src() const;
		Endpoint* get_sink0() const;
		Endpoints get_sinks() const;
		const Endpoints& sinks() const;
		void set_src(Endpoint*);
		void add_sink(Endpoint*);
		void remove_sink(Endpoint*);
		bool has_sink(Endpoint*) const;

		PROP_GET(net_type, const NetworkID&, m_net_type);

	protected:
		const NetworkID& m_net_type;
		Endpoint* m_src;
		Endpoints m_sinks;
	};
}
