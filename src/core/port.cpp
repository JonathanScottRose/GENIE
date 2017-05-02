#include "pch.h"
#include "port.h"
#include "node.h"

using namespace genie::impl;

//
// Public
//

genie::Port::Dir Port::get_dir() const
{
    return m_dir;
}

genie::Port::Dir genie::Port::Dir::rev() const
{
	Dir d = *this; // kludge
	switch (d)
	{
	case Dir::IN: return Dir::OUT; break;
	case Dir::OUT: return Dir::IN; break;
	default: assert(false);
	}

	return Dir::OUT;
}

//
// Internal
//

Port::~Port()
{
}

Port::Port(const std::string & name, Dir dir)
    : m_dir(dir)
{
    set_name(name);
}

Port::Port(const Port& o)
    : HierObject(o), m_dir(o.m_dir)
{
    // Copy sub-ports, if any
    for (auto c : o.get_children_by_type<Port>())
    {
        add_child(c->clone());
    }
}

Node * Port::get_node() const
{
    Node* result = nullptr;
    HierObject* cur = const_cast<Port*>(this);

    // Get the nearest parent that's a Node
    while (cur && (result = dynamic_cast<Node*>(cur)) == nullptr)
        cur = cur->get_parent();

    return result;
}

genie::Port::Dir Port::get_effective_dir(Node * contain_ctx) const
{
	// Get this Port's effective dir within a containing Node/System.
	// ex: an 'input' port attached to a System effectively becomes an output
	// when connecting that Port to other ports within the System.
	// It's a way to deal with the two-sidedness of ports.

	// The effective dir is only reversed when this port lies on the boundary
	// of the containing Node. Otherwise, it is assumed that the port belongs
	// to a node WITHIN contain_ctx, in which case its nominal direction is respected.
	auto result = get_dir();
	auto owning_node = get_node();

	if (owning_node == contain_ctx)
		result = result.rev();
	else
		assert(owning_node->get_parent() == contain_ctx);

	return result;
}

//
// SubPort
//

SubPortBase::SubPortBase(const std::string & name, genie::Port::Dir dir)
    : Port(name, dir)
{
}

void SubPortBase::resolve_params(ParamResolver& r)
{
    m_binding.resolve_params(r);
}

