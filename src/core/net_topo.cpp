#include "genie/net_topo.h"
#include "genie/genie.h"

using namespace genie;

namespace
{
	// Define network
	class NetTopoDef : public NetTypeDef
	{
	public:
		NetTopoDef(NetType id)
			: NetTypeDef(id)
		{
			m_name = "topo";
			m_desc = "Topology";
			m_has_port = false;
			m_src_multibind = true;
			m_sink_multibind = true;
			m_ep_asp_id = Aspect::asp_id_of<ATopoEndpoint>();
		}

		~NetTopoDef()
		{
		}

		Link* create_link()
		{
			return new TopoLink();
		}

		AEndpoint* create_endpoint(Dir dir, HierObject* container)
		{
			return new ATopoEndpoint(dir, container);
		}
	};
}

// Register the network type
const NetType genie::NET_TOPO = NetTypeDef::add<NetTopoDef>();

//
// ATopoEndpoint
//

ATopoEndpoint::ATopoEndpoint(Dir dir, HierObject* container)
: AEndpoint(dir, container)
{
}

ATopoEndpoint::~ATopoEndpoint()
{
}

