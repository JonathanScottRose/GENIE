#pragma once

#include "node.h"

namespace genie
{
namespace impl
{
	class PortRS;

    class NodeSplit : public Node
    {
    public:
        static void init();
        
        // Create a new one
		NodeSplit();
		~NodeSplit();

        // Generic copy of an existing one
        NodeSplit* clone() const override;

		PROP_GET(n_outputs, unsigned, m_n_outputs);
		void create_ports();
		PortRS* get_input() const;
		PortRS* get_output(unsigned) const;

    protected:
        NodeSplit(const NodeSplit&);

		void init_vlog();

		unsigned m_n_outputs;
    };
}
}