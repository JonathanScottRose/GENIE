#pragma once

#include "node.h"
#include "protocol.h"
#include "genie_priv.h"

namespace genie
{
namespace impl
{
	class PortRS;

	extern FieldType FIELD_SPLITMASK;

    class NodeSplit : public Node, public ProtocolCarrier
    {
    public:
        static void init();
        
        // Create a new one
		NodeSplit();
		~NodeSplit();

        // Generic copy of an existing one
        NodeSplit* clone() const override;
		void prepare_for_hdl() override;
		void annotate_timing() override;
		AreaMetrics annotate_area() override;

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