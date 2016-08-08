#include "genie/net_topo.h"
#include "genie/net_rvd.h"
#include "genie/genie.h"

using namespace genie;

NetType genie::NET_TOPO = NET_INVALID;

void NetTopo::init()
{
    NET_TOPO = Network::reg<NetTopo>();
}

NetTopo::NetTopo()
{
	m_name = "topo";
	m_desc = "Topology";
	m_default_max_in = 1;
	m_default_max_out = 1;
}

Link* NetTopo::create_link()
{
	return new TopoLink();
}

Port* NetTopo::create_port(Dir dir)
{
	return new TopoPort(dir);
}

//
// TopoPort
//

TopoPort::TopoPort(Dir dir)
	: Port(dir, NET_TOPO), m_n_rvd(0)
{
    asp_add(new ALinkContainment());
}

TopoPort::TopoPort(Dir dir, const std::string& name)
	: TopoPort(dir)
{
	set_name(name);
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
    m_n_rvd = 0;
    for (LinkFace face : {LinkFace::INNER, LinkFace::OUTER})
    {
        auto ep = get_endpoint(NET_TOPO, face);
        m_n_rvd = std::max(m_n_rvd, (int)ep->links().size());
    }
	
	for (int i = 0; i < m_n_rvd; i++)
	{
		RVDPort* subp = new RVDPort(this->get_dir());
		std::string subp_name = genie::hier_make_reserved_name("rvd");
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
		throw HierException(this, "subport out of range: " + std::to_string(i));

	std::string subp_name = genie::hier_make_reserved_name("rvd");
	subp_name += std::to_string(i);

	return as_a<RVDPort*>(get_child(subp_name));
}

HierObject* TopoPort::instantiate()
{
	return new TopoPort(*this);
}

genie::TopoLink::TopoLink()
{
    asp_add(new ALinkContainment());
}

Link* TopoLink::clone() const
{
    auto result = new TopoLink(*this);
    result->asp_add(new ALinkContainment());
    return result;
}