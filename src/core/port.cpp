#include "pch.h"
#include "port.h"
#include "node.h"
#include "sig_role.h"

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
    : HierObject(o), m_dir(o.m_dir), m_role_bindings(o.m_role_bindings)
{
}

HierObject * Port::instantiate() const
{
	// default is just a clone
	return this->clone();
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

const std::vector<Port::RoleBinding>& Port::get_role_bindings() const
{
	return m_role_bindings;
}

std::vector<Port::RoleBinding> Port::get_role_bindings(SigRoleType rtype)
{
	std::vector<RoleBinding> result;

	std::copy_if(m_role_bindings.begin(), m_role_bindings.end(), result.end(),
		[&](const RoleBinding& rb)
	{
		return rb.role.type == rtype;
	});

	return result;
}

Port::RoleBinding* Port::get_role_binding(const SigRoleID& role)
{
	auto it = std::find_if(m_role_bindings.begin(), m_role_bindings.end(),
		[&](const RoleBinding& rb)
	{
		return rb.role == role;
	});

	return (it == m_role_bindings.end()) ? nullptr : &(*it);
}


Port::RoleBinding& Port::add_role_binding(const SigRoleID& role,
	const hdl::PortBindingRef & bnd)
{
	auto roledef = genie::impl::get_sig_role(role.type);

	// Check to make sure a tag provided if and only if required
	if (roledef->uses_tags() && role.tag.empty())
	{
		throw Exception(get_hier_path() + ": role " +
			roledef->get_name() + " requires a tag");
	}
	else if (!roledef->uses_tags() && !role.tag.empty())
	{
		throw Exception(get_hier_path() + ": role " +
			roledef->get_name() + " must have empty tag");
	}

	// Create the role binding
	if (get_role_binding(role) != nullptr)
	{
		throw Exception(get_hier_path() + ": role " + roledef->get_name() + " with tag '"
			+ role.tag + " already exists");
	}

	m_role_bindings.push_back(RoleBinding{ role, bnd });
	return m_role_bindings.back();
}

void Port::add_signal(const SigRoleID& role, const std::string & sig_name,
	const std::string & width)
{
	HDLPortSpec ps;
	ps.name = sig_name;
	ps.width = width;
	ps.depth = "1";

	HDLBindSpec bs;
	bs.width = width;
	bs.depth = "1";
	bs.lo_bit = "0";
	bs.lo_slice = "0";

	add_signal_ex(role, ps, bs);
}

void Port::add_signal_ex(const SigRoleID& role, const genie::HDLPortSpec& pspec,
	const genie::HDLBindSpec& bspec)
{
	// Check if this role type is allowed for this port type
	auto portdef = genie::impl::get_port_type(get_type());
	auto roledef = genie::impl::get_sig_role(role.type);

	if (!portdef->has_role(role.type))
	{
		throw Exception(get_name() + ": invalid signal role type " + roledef->get_name());
	}

	// Determine HDL port dir based on the sense of the role, and the dir of the port
	hdl::Port::Dir hdl_dir;

	switch (roledef->get_sense())
	{
	case SigRoleDef::FWD:
		hdl_dir = hdl::Port::Dir::from_logical(get_dir());
		break;
	case SigRoleDef::REV:
		hdl_dir = hdl::Port::Dir::from_logical(get_dir()).rev();
		break;
	case SigRoleDef::IN:
		hdl_dir = hdl::Port::Dir::IN;
		break;
	case SigRoleDef::OUT:
		hdl_dir = hdl::Port::Dir::OUT;
		break;
	case SigRoleDef::INOUT:
		hdl_dir = hdl::Port::Dir::INOUT;
		break;
	default:
		assert(false);
	}

	// Make sure the port is rooted inside a node first, because we will need to access
	// its HDL state to actually create the HDL port
	auto node = get_node();
	if (!node)
		throw Exception(get_name() + ": port not attached to node yet");

	// Get or create HDL port
	auto& hdl_state = node->get_hdl_state();
	auto& hdl_port = hdl_state.get_or_create_port(pspec.name, pspec.width, pspec.depth, hdl_dir);

	// Create a PortBindingRef from the public bspec/pspec structs
	hdl::PortBindingRef hdl_pb;
	hdl_pb.set_port_name(pspec.name);
	hdl_pb.set_bits(bspec.width);
	hdl_pb.set_lo_bit(bspec.lo_bit);
	hdl_pb.set_slices(bspec.depth);
	hdl_pb.set_lo_slice(bspec.lo_slice);

	add_role_binding(role, hdl_pb);
}

void Port::resolve_params(ParamResolver& r)
{
	for (auto& b : m_role_bindings)
		b.binding.resolve_params(r);
}

void PortTypeDef::set_roles(const std::initializer_list<const char*>& roles)
{
	m_roles = roles;
}

bool PortTypeDef::has_role(SigRoleType role) const
{
	for (auto str : m_roles)
	{
		if (!strcmp(str, role.to_string().c_str()))
			return true;
	}

	return false;
}
