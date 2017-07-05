#pragma once

#include "node.h"
#include "address.h"
#include "protocol.h"

namespace genie
{
namespace impl
{
	class PortRS;

	class NodeConv: public Node, public ProtocolCarrier
	{
	public:
		static void init();

		// Create a new one
		NodeConv();

		// Generic copy of an existing one
		NodeConv* clone() const override;
		void prepare_for_hdl() override;
		void annotate_timing() override;
		AreaMetrics annotate_area() override;

		PortRS* get_input() const;
		PortRS* get_output() const;

		void configure(const AddressRep& in_rep, const FieldID& in_field,
			const AddressRep& out_rep, const FieldID& out_field);

	protected:
		NodeConv(const NodeConv&);

		void init_vlog();

		std::vector<std::pair<unsigned, unsigned>> m_table;
		unsigned m_in_width;
		unsigned m_out_width;
	};
}
}