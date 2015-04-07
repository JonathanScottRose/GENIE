#include "genie/vlog_bind.h"

using namespace genie::vlog;

//
// AVlogInfo
//

AVlogInfo::AVlogInfo()
	: m_module(nullptr), m_instance(nullptr)
{
}

AVlogInfo::~AVlogInfo()
{
	// we do not own m_module, don't delete it
	delete m_instance;
}

genie::Aspect* AVlogInfo::asp_instantiate()
{
	auto result = new AVlogInfo();
	result->set_module(m_module);
	
	return result;
}

//
// VlogStaticBinding
//

VlogStaticBinding::VlogStaticBinding()
	: m_port(nullptr)
{
}

VlogStaticBinding::VlogStaticBinding(Port* port, const Expression& width, const Expression& lo)
	: m_port(port), m_width(width), m_lo(lo)
{
}

VlogStaticBinding::VlogStaticBinding(Port* port, const Expression& width)
	: VlogStaticBinding(port, width, 0)
{
}

VlogStaticBinding::VlogStaticBinding(Port* port)
	: VlogStaticBinding(port, port->get_width())
{
}

genie::HDLBinding* VlogStaticBinding::clone()
{
	return new VlogStaticBinding(*this);
}

int VlogStaticBinding::get_lo() const
{
	// evaluate expression using parent Node
	Node* node = get_parent()->get_parent()->get_node();
	assert(node);
	return m_lo.get_value(node->get_exp_resolver());
}

int VlogStaticBinding::get_width() const
{
	Node* node = get_parent()->get_parent()->get_node();
	assert(node);
	return m_width.get_value(node->get_exp_resolver());
}

Port* VlogStaticBinding::get_port() const
{
	return m_port;
}

std::string VlogStaticBinding::to_string() const
{
	if (!m_port)
		return "(unbound)";
	else
		return m_port->get_name();
}

genie::HDLBinding* VlogStaticBinding::export_pre()
{
	// Create a new binding, with a concrete width and a 0 lobase.
	// Keep the port the same as the old one, it will be used in post()
	auto result = new VlogStaticBinding();
	result->set_lo(0);
	result->set_width(this->get_width());
	result->set_port(this->get_port());

	return result;
}

void VlogStaticBinding::export_post()
{
	// We should be attached by now
	assert(m_parent);

	// Get our (new) parent node's Vlog module
	RoleBinding* rb = get_parent();
	Node* node = rb->get_parent()->get_node();

	auto av = node->asp_get<AVlogInfo>();
	assert(av);
	Module* mod = av->get_module();

	// This was set in _pre(). Create a new port, but keep the old one's direction.
	Port* oldport = get_port();
	Port* newport = new Port(vlog::make_default_port_name(rb), 
		get_width(), oldport->get_dir());
	set_port(newport);

	// Add the port to the module
	mod->add_port(newport);
}