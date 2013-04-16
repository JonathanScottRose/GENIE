#pragma once

#include <vector>
#include <unordered_map>
#include "at_protocol.h"
#include "at_spec.h"
#include "ati_channel.h"

class ATNetlist;
class ATNetNode;
class ATNetPort;
class ATNetInPort;
class ATNetOutPort;
class ATNetFlow;
class ATNet;


class ATNetlist
{
public:
	typedef std::unordered_map<std::string, ATNetNode*> NodeMap;
	typedef std::vector<ATNetFlow*> FlowGroup;
	typedef std::unordered_map<int, FlowGroup> FlowMap;

	static void connect_net_to_inport(ATNet* net, ATNetInPort* inport);
	static void connect_outport_to_net(ATNetOutPort* outport, ATNet* net);

	ATNetlist();
	~ATNetlist();

	const NodeMap& nodes() { return m_nodes; }
	ATNetNode* get_node(const std::string& name) { return m_nodes[name]; }
	void add_node(ATNetNode* node);
	void remove_node(ATNetNode* node);

	const FlowMap& flows() { return m_flows; }
	const FlowGroup& get_flow(int id) { return m_flows[id]; }
	void add_flow(ATNetFlow* flow);

	ATNetNode* get_node_for_linkpoint(const ATLinkPointDef& lp_def);

	void dump_graph();

private:
	NodeMap m_nodes;
	FlowMap m_flows;
};


class ATNetNode
{
public:
	typedef std::unordered_map<std::string, ATNetInPort*> InPortMap;
	typedef std::unordered_map<std::string, ATNetOutPort*> OutPortMap;

	enum Type
	{
		INSTANCE,
		ARB,
		BCAST
	};

	ATNetNode();
	virtual ~ATNetNode();

	void set_name(const std::string& name) { m_name = name; }
	const std::string& get_name() { return m_name; }

	const InPortMap& inports() { return m_inports; }
	ATNetInPort* get_inport(const std::string& name) { return m_inports[name]; }

	const OutPortMap& outports() { return m_outports; }
	ATNetOutPort* get_outport(const std::string& name) { return m_outports[name]; }

	Type get_type() { return m_type; }
	virtual void instantiate() = 0;
	
protected:
	void add_inport(ATNetInPort* port);
	void add_outport(ATNetOutPort* port);

	Type m_type;
	std::string m_name;
	InPortMap m_inports;
	OutPortMap m_outports;
};


class ATNetPort
{
public:
	typedef std::vector<ATNetFlow*> FlowVec;

	ATNetPort(ATNetNode* node);
	~ATNetPort();

	ATNetNode* get_parent() { return m_parent; }

	void set_name(const std::string& name) { m_name = name; }
	const std::string& get_name() { return m_name; }

	void set_proto(const ATLinkProtocol& p) { m_proto = p; }
	const ATLinkProtocol& get_proto() { return m_proto; }

	void set_clock(const std::string& clk) { m_clk = clk; }
	const std::string& get_clock() { return m_clk; }

	void set_net(ATNet* net) { m_net = net; }
	ATNet* get_net() { return m_net; }

	const FlowVec& flows() { return m_flows; }
	void add_flow(ATNetFlow* f);
	void remove_flow(ATNetFlow* f);
	bool has_flow(ATNetFlow* f);
	void clear_flows();
	void add_flows(const FlowVec& f);

protected:
	std::string m_name;
	ATNetNode* m_parent;	
	ATLinkProtocol m_proto;
	std::string m_clk;
	ATNet* m_net;
	FlowVec m_flows;
};


class ATNetInPort : public ATNetPort
{
public:
	ATNetInPort(ATNetNode* node) : ATNetPort(node) { }

	void set_impl(ati_recv* s) { m_impl = s; }
	ati_recv* get_impl() { return m_impl; }

protected:
	ati_recv* m_impl;
};


class ATNetOutPort : public ATNetPort
{
public:
	ATNetOutPort(ATNetNode* node) : ATNetPort(node) { }

	void set_impl(ati_send* s) { m_impl = s; }
	ati_send* get_impl() { return m_impl; }

protected:
	ati_send* m_impl;
};


class ATNet
{
public:
	typedef std::vector<ATNetInPort*> PortVec;

	ATNet();
	ATNet(ATNetOutPort* driver);

	void set_driver(ATNetOutPort* p) { m_driver = p; }
	ATNetOutPort* get_driver() { return m_driver; }

	const PortVec& fanout() { return m_fanout; }
	void add_fanout(ATNetInPort* p);
	void remove_fanout(ATNetInPort* p);
	
protected:
	ATNetOutPort* m_driver;
	PortVec m_fanout;
};


class ATNetFlow
{
public:
	void set_src_port(ATNetOutPort* port) { m_src_port = port; }
	ATNetOutPort* get_src_port() { return m_src_port; }

	void set_dest_port(ATNetInPort* port) { m_dest_port = port; }
	ATNetInPort* get_dest_port() { return m_dest_port;}

	void set_def(ATLinkDef* def) { m_link_def = def; }
	ATLinkDef* get_def() { return m_link_def; }

	int get_id() { return m_id; }
	void set_id(int id) { m_id = id; }

	bool same_phys_dest(ATNetFlow* other);

protected:
	ATNetOutPort* m_src_port;
	ATNetInPort* m_dest_port;
	ATLinkDef* m_link_def;
	int m_id;
};