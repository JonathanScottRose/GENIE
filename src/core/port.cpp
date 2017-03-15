#include "pch.h"
#include "port.h"

using namespace genie::impl;

//
// Public
//

const std::string & Port::get_name() const
{
    return HierObject::get_name();
}

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
    : HierObject(o)
{
    // Copy sub-ports, if any
    for (auto c : o.get_children_by_type<Port>())
    {
        add_child(c->instantiate());
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

