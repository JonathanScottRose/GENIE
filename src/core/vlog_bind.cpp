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
