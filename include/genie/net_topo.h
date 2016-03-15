#pragma once

#include "genie/networks.h"
#include "genie/structure.h"
#include "genie/net_rvd.h"

namespace genie
{
	extern const NetType NET_TOPO;

	class TopoPort : public Port
	{
	public:
		TopoPort(Dir dir);
		TopoPort(Dir dir, const std::string& name);
		~TopoPort();

		int get_n_rvd_ports() const;
		RVDPort* get_rvd_port(int = 0) const;

		void refine(NetType) override;

		HierObject* instantiate() override;

	protected:
		int m_n_rvd;
	};

    class TopoLink : public Link
    {
    public:
        PROP_GET_SET(latency, int, m_latency);

    protected:
        int m_latency = 0;
    };
}