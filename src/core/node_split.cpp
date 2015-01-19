#include "genie/node_split.h"
#include "genie/net_rvd.h"
#include "genie/net_clock.h"
#include "genie/net_reset.h"
#include "genie/net_topo.h"
#include "genie/genie.h"

using namespace genie;

namespace
{
	const std::string BUILTIN_NAME = "genie_split";
	const std::string INPORT_NAME = "in";
	const std::string OUTPORT_NAME = "out";
	const std::string CLOCKPORT_NAME = "clock";
	const std::string RESETPORT_NAME = "reset";

	class SplitNodeDef : public NodeDef
	{
	public:
		SplitNodeDef()
			: NodeDef(BUILTIN_NAME)
		{
			add_port_def(new ClockPortDef(Dir::IN, CLOCKPORT_NAME, this));
			add_port_def(new ResetPortDef(Dir::IN, RESETPORT_NAME, this));
			add_port_def(new RVDPortDef(Dir::IN, INPORT_NAME, this));
			add_port_def(new RVDPortDef(Dir::OUT, OUTPORT_NAME, this));
		}

		~SplitNodeDef() = default;

		HierObject* instantiate()
		{
			return new SplitNode();
		}
	};

	RegisterBuiltin<SplitNodeDef> s_reg;
}

NodeDef* SplitNode::prototype()
{
	return (SplitNodeDef*)genie::get_root()->get_object(BUILTIN_NAME);
}


SplitNode::SplitNode()
: m_topo_finalized(false), m_outputs(nullptr)
{
	auto ndef = (SplitNodeDef*)prototype();
	set_prototype(ndef);	

	Port* p;
	
	// Clock and reset ports are straightforward
	p = new ClockPort(Dir::IN, CLOCKPORT_NAME, this);
	p->set_prototype(ndef->get_port_def(CLOCKPORT_NAME));
	add_port(p);

	p = new ResetPort(Dir::IN, RESETPORT_NAME, this);
	p->set_prototype(ndef->get_port_def(RESETPORT_NAME));
	add_port(p);

	// Input port
	p = new RVDPort(Dir::IN, INPORT_NAME, this);
	p->set_prototype(ndef->get_port_def(INPORT_NAME));
	p->asp_add(new ATopoEndpoint(Dir::IN, p));

	// Output port is not a real port yet - just a topo endpoint
	HierObject* q = new HierObject(OUTPORT_NAME);
	q->asp_add(new ATopoEndpoint(Dir::OUT, q));

	asp_get<AHierParent>()->add_child(q);	
}

SplitNode::SplitNode(const std::string& name, System* parent)
: SplitNode()
{
	set_name(name);
	set_parent(parent);
}

SplitNode::~SplitNode()
{
	delete[] m_outputs;
}

int SplitNode::get_n_outputs() const
{
	if (!m_topo_finalized)
		throw HierException(this, "Split Node not elaborated yet");

	return m_n_outputs;
}

Port* SplitNode::get_input() const
{
	return get_port(INPORT_NAME);
}

Port* SplitNode::get_output(int idx) const
{
	if (!m_topo_finalized)
		throw HierException(this, "Split Node not elaborated yet");

	return m_outputs[idx];
}