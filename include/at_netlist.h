#pragma once

#include <vector>
#include <map>
#include "at_protocol.h"
#include "at_spec.h"
#include "ati_channel.h"

class ATInstanceNode;

class ATNetlist
{
public:
	typedef std::vector<class ATNetNode*> NodeVec;

	ATNetlist();
	~ATNetlist();

	const NodeVec& nodes() { return m_nodes; }
	void add_node(ATNetNode* node);
	void remove_node(ATNetNode* node);
	ATInstanceNode* get_instance_node(const std::string& name);
	void clear();

private:
	typedef std::map<std::string, ATInstanceNode*> InstNodeMap;

	NodeVec m_nodes;
	InstNodeMap m_inst_node_map;
};


class ATNetNode
{
public:
	typedef std::vector<class ATNetInPort*> InPortVec;
	typedef std::vector<class ATNetOutPort*> OutPortVec;

	enum Type
	{
		INSTANCE,
		ARB,
		BCAST,
		ADAPTER
	};

	ATNetNode();
	virtual ~ATNetNode();

	InPortVec& get_inports() { return m_inports; }
	OutPortVec& get_outports() { return m_outports; }
	Type get_type() { return m_type; }

	virtual void instantiate() = 0;
	
protected:
	Type m_type;
	InPortVec m_inports;
	OutPortVec m_outports;
};


class ATNetInPort
{
public:
	typedef std::vector<class ATNetOutPort*> FaninVec;

	ATNetInPort(ATNetNode* node);

	FaninVec& get_fanin() { return m_fanin; }
	ATNetNode* get_node() { return m_node; }

	const ATLinkProtocol& get_proto() { return m_proto; }
	void set_proto(const ATLinkProtocol& p) { m_proto = p; }

	void set_clock(const std::string& clk) { m_clk = clk; }
	const std::string& get_clock() { return m_clk; }

	void set_impl(ati_recv* s) { m_impl = s; }
	ati_recv* get_impl() { return m_impl; }

	void set_addr(int a) { m_addr = a; }
	int get_addr() { return m_addr; }

protected:
	ATLinkProtocol m_proto;
	FaninVec m_fanin;
	std::string m_clk;
	ati_recv* m_impl;
	ATNetNode* m_node;
	int m_addr;
};


class ATNetOutPort
{
public:
	typedef std::vector<class ATNetInPort*> FanoutVec;

	ATNetOutPort(ATNetNode* node);

	FanoutVec& get_fanout() { return m_fanout; }
	ATNetNode* get_node() { return m_node; }

	const ATLinkProtocol& get_proto() { return m_proto; }
	void set_proto(const ATLinkProtocol& p) { m_proto = p; }

	void set_clock(const std::string& clk) { m_clk = clk; }
	const std::string& get_clock() { return m_clk; }

	void set_impl(ati_send* s) { m_impl = s; }
	ati_send* get_impl() { return m_impl; }

	void set_addr(int a) { m_addr = a; }
	int get_addr() { return m_addr; }

private:
	ATLinkProtocol m_proto;
	FanoutVec m_fanout;
	std::string m_clk;
	ati_send* m_impl;
	ATNetNode* m_node;
	int m_addr;
};
