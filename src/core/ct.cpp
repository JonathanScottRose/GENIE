#include "ct/ct.h"
#include "ct/structure.h"

using namespace ct;

namespace
{
	Registry s_registry;
}

//
// Registry
//

Registry::Registry()
: HierContainer()
{
	hier_add(new HierContainer(NetworkDef::ID, this));
	hier_add(new HierContainer(System::ID, this));
	hier_add(new HierContainer(NodeDef::ID, this));
}

Registry::~Registry()
{
}

void Registry::reg_net_def(NetworkDef* def)
{
	hier_child_as<HierContainer*>(NetworkDef::ID)->hier_add(def);
}

NetworkDef* Registry::get_net_def(const NetworkID& id) const
{
	return hier_child(NetworkDef::ID)->hier_child_as<NetworkDef*>(id);
}

void Registry::reg_system(System* sys)
{
	hier_child_as<HierContainer*>(System::ID)->hier_add(sys);
}

System* Registry::get_system(const std::string& name) const
{
	return hier_child(System::ID)->hier_child_as<System*>(name);
}

List<System*> Registry::get_systems() const
{
	return hier_child(System::ID)->hier_children_as<System*>();
}

void Registry::reg_node_def(NodeDef* def)
{
	hier_child_as<HierContainer*>(NodeDef::ID)->hier_add(def);
}

NodeDef* Registry::get_node_def(const std::string& name) const
{
	return hier_child(NodeDef::ID)->hier_child_as<NodeDef*>(name);
}

List<NodeDef*> Registry::get_node_defs() const
{
	return hier_child(NodeDef::ID)->hier_children_as<NodeDef*>();
}

//
// Public functions
//

Registry* ct::get_root()
{
	return &s_registry;
}
