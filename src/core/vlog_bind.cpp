#include "genie/vlog_bind.h"

using namespace genie::vlog;


//
// VlogStaticBinding
//

VlogStaticBinding::VlogStaticBinding()
	: m_full_width(true)
{
}

VlogStaticBinding::VlogStaticBinding(const std::string& portname, 
	const Expression& width, const Expression& lsb)
	: m_port_name(portname), m_width(width), m_lsb(lsb), m_full_width(false)
{
}

VlogStaticBinding::VlogStaticBinding(const std::string& portname, const Expression& width)
	: VlogStaticBinding(portname, width, 0)
{
}

VlogStaticBinding::VlogStaticBinding(const std::string& portname)
	: m_port_name(portname), m_full_width(true), m_lsb(0)
{
}

genie::HDLBinding* VlogStaticBinding::clone()
{
	return new VlogStaticBinding(*this);
}

int VlogStaticBinding::get_lsb() const
{
	// evaluate expression using parent Node to plug-in parameter values
	Node* node = get_parent()->get_parent()->get_node();
	assert(node);
	return m_lsb.get_value(node->get_exp_resolver());
}

int VlogStaticBinding::get_width() const
{
	// If we're a full-width binding, our m_width is useless and we need to query the port.
	const Expression& widthexpr = m_full_width? get_port()->get_width() : m_width;

	Node* node = get_parent()->get_parent()->get_node();
	assert(node);
	return widthexpr.get_value(node->get_exp_resolver());
}

Port* VlogStaticBinding::get_port() const
{
	// We have the port's name, so we just need to look up the actual Port in the Node's
	// NodeHDLInfo thingy.
	Node* node = get_parent()->get_parent()->get_node();
	assert(node);

	auto vinfo = as_a<NodeVlogInfo*>(node->get_hdl_info());
	assert(vinfo);

	return vinfo->get_port(m_port_name);
}

std::string VlogStaticBinding::to_string() const
{
	return m_port_name;
}

void VlogStaticBinding::set_lsb(const Expression& e)
{
	m_lsb = e;
	m_full_width = false;
}

void VlogStaticBinding::set_width(const Expression& e)
{
	m_width = e;
	m_full_width = false;
}
