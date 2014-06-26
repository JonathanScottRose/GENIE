#include "ct/networks.h"
#include "ct/ct.h"

using namespace ct;

const std::string NetworkDef::ID = "network";

NetworkDef::NetworkDef(const NetworkID& id)
: m_id(id)
{
}

const std::string& NetworkDef::hier_name() const
{
	return m_id;
}

HierNode* NetworkDef::hier_parent() const
{
	return ct::get_root()->hier_child(NetworkDef::ID);
}

