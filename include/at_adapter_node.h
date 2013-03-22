#pragma once

#include "at_netlist.h"
#include "at_protocol.h"

class ATAdapterNode : public ATNetNode
{
public:
	ATAdapterNode(const ATLinkProtocol& src_proto, const ATLinkProtocol& dest_proto);
	~ATAdapterNode();

	void instantiate();

protected:
	ATLinkProtocol m_src_proto;
	ATLinkProtocol m_dest_proto;
};
