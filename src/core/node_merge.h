#pragma once

#include "node.h"

namespace genie
{
namespace impl
{
	class PortRS;

    class NodeMerge : public Node
    {
    public:
        static void init();
        
        // Create a new one
		NodeMerge();

        // Generic copy of an existing one
        NodeMerge* clone() const override;

		PROP_GET(n_inputs, unsigned, m_n_inputs);
		void create_ports();
		PortRS* get_input(unsigned) const;
		PortRS* get_output() const;

    protected:
		NodeMerge(const NodeMerge&);

		void init_vlog();

		unsigned m_n_inputs;
    };
}
}