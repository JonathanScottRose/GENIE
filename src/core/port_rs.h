#pragma once

#include "prop_macros.h"
#include "port.h"
#include "protocol.h"
#include "genie/port.h"

namespace genie
{
namespace impl
{
	class PortClock;
	class NodeSystem;
    class PortRS;

	extern PortType PORT_RS;

	extern FieldType FIELD_USERDATA;
	extern FieldType FIELD_USERADDR;
	extern FieldType FIELD_XMIS_ID;
	extern FieldType FIELD_EOP;

	// Backpressure capabilities
	struct RSBackpressure
	{
		bool configurable;
		enum Status
		{
			UNSET,
			ENABLED,
			DISABLED
		} status;

		RSBackpressure() : configurable(false), status(UNSET) {}
		void make_configurable() { configurable = true; status = UNSET; }
		void force_enable() { configurable = false; status = ENABLED; }
		void force_disable() { configurable = false; status = DISABLED; }
	};

    class PortRS : virtual public genie::PortRS, public Port
    {
    public:
		void set_logic_depth(unsigned) override;
		void set_default_packet_size(unsigned) override;
		void set_default_importance(float) override;

    public:
		static SigRoleType DATA_CARRIER;

		static void init();

        PortRS(const std::string& name, genie::Port::Dir dir);
		PortRS(const std::string& name, genie::Port::Dir dir, const std::string& clk_port_name);
		PortRS(const PortRS&) = default;
        ~PortRS();

        PortRS* clone() const override;
		Port* export_port(const std::string& name, NodeSystem*) override;
		PortType get_type() const override { return PORT_RS; }
		void reintegrate(HierObject*) override;
		HierObject* instantiate() const override;

		PROP_GET(default_packet_size, unsigned, m_default_pkt_size);
		PROP_GET(default_importance, float, m_default_importance);
		PROP_GET(logic_depth, unsigned, m_logic_depth);
        PROP_GET_SET(clock_port_name, const std::string&, m_clk_port_name);
		PROP_GET_SETR(proto, PortProtocol&, m_proto);
		PROP_GETR(bp_status, RSBackpressure&, m_bp_status);

		PortClock* get_clock_port() const;

		CarrierProtocol* get_carried_proto() const;
		bool has_field(const FieldID&) const;
		FieldInst* get_field(const FieldID&);

    protected:
		float m_default_importance; // only for user specification
		unsigned m_default_pkt_size; // only for user specification
		unsigned m_logic_depth;
        std::string m_clk_port_name;
		PortProtocol m_proto;
		RSBackpressure m_bp_status;
    };
}
}