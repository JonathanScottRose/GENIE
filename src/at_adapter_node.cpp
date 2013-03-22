#include "at_adapter_node.h"
#include "ati_proto_adapter.h"
#include "at_instance_node.h"

ATAdapterNode::ATAdapterNode(const ATLinkProtocol& src_proto, const ATLinkProtocol& dest_proto)
: m_src_proto(src_proto), m_dest_proto(dest_proto)
{
	m_type = ATNetNode::ADAPTER;

	ATNetInPort* inport = new ATNetInPort(this);
	inport->set_proto(src_proto);
	m_inports.push_back(inport);

	ATNetOutPort* outport = new ATNetOutPort(this);
	outport->set_proto(dest_proto);
	m_outports.push_back(outport);
}


ATAdapterNode::~ATAdapterNode()
{
}


void ATAdapterNode::instantiate()
{
	ati_proto_adapter* ad = new ati_proto_adapter(
		sc_gen_unique_name("ati_proto_adapter"), m_src_proto, m_dest_proto);

	m_inports.front()->set_impl(&ad->i_in);
	m_outports.front()->set_impl(&ad->o_out);

	if (m_src_proto.addr_width == 0 && m_dest_proto.addr_width > 0)
	{
		ATNetNode* dest_node = m_outports.front()->get_fanout().front()->get_node();
		assert(dest_node->get_type() == ATNetNode::INSTANCE);
		ATInstanceNode* idest_node = (ATInstanceNode*) dest_node;

		sc_bv_base addr(m_dest_proto.addr_width);
		addr = 0;

		ad->set_addr(addr);
	}
}
