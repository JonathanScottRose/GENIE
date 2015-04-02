#include "genie/net_topo.h"
#include "genie/net_rvd.h"
#include "genie/genie.h"

using namespace genie;

namespace
{
	// Define network
	class NetTopo : public Network
	{
	public:
		NetTopo(NetType id)
			: Network(id)
		{
			m_name = "topo";
			m_desc = "Topology";
			m_src_multibind = true;
			m_sink_multibind = true;
		}

		~NetTopo()
		{
		}

		Link* create_link()
		{
			auto result = new Link();
			result->asp_add(new ALinkContainment());
			return result;
		}

		Port* create_port(Dir dir)
		{
			return new TopoPort(dir);
		}
	};
}

// Register the network type
const NetType genie::NET_TOPO = Network::add<NetTopo>();

//
// TopoPort
//

TopoPort::TopoPort(Dir dir)
	: Port(dir, NET_TOPO), m_n_rvd(0)
{
	set_connectable(NET_TOPO, dir);
}

TopoPort::TopoPort(Dir dir, const std::string& name)
	: TopoPort(dir)
{
	set_name(name);
}

HierObject* TopoPort::instantiate()
{
	return new TopoPort(*this);
}

TopoPort::~TopoPort()
{
}

void TopoPort::refine(NetType nettype)
{
	// Call default first
	HierObject::refine(nettype);

	if (nettype != NET_RVD)
		return;

	// Make an RVD for each connected TOPO link
	auto ep = get_endpoint(NET_TOPO, LinkFace::OUTER);
	
	m_n_rvd = ep->links().size();
	for (int i = 0; i < m_n_rvd; i++)
	{
		RVDPort* subp = new RVDPort(this->get_dir());
		std::string subp_name = genie::make_reserved_name("rvd");
		subp_name += std::to_string(i);
		subp->set_name(subp_name, true);

		add_child(subp);
	}
}

int TopoPort::get_n_rvd_ports() const
{
	return m_n_rvd;
}

RVDPort* TopoPort::get_rvd_port(int i) const
{
	if (i < 0 || i >= m_n_rvd)
		throw HierException(this, "subport out of range: " + i);

	std::string subp_name = genie::make_reserved_name("rvd");
	subp_name += std::to_string(i);

	return as_a<RVDPort*>(get_child(subp_name));
}

