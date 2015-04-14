#include "genie/sigroles.h"
#include "genie/networks.h"
#include "genie/structure.h"

using namespace genie;

RoleBinding::RoleBinding(SigRoleID id, const std::string& tag, HDLBinding* hdl)
	: m_id(id), m_tag(tag), m_parent(nullptr)
{
	set_hdl_binding(hdl);
}

RoleBinding::RoleBinding(SigRoleID id, HDLBinding* hdl)
	: RoleBinding(id, "", hdl)
{
}

RoleBinding::RoleBinding(const RoleBinding& o)
	: RoleBinding(o.m_id, o.m_tag)
{ 
	set_hdl_binding(o.m_binding? o.m_binding->clone() : nullptr);
}

RoleBinding::~RoleBinding()
{
	delete m_binding;
}

void RoleBinding::set_hdl_binding(HDLBinding* b)
{
	delete m_binding;
	m_binding = b;
	if (m_binding)
		m_binding->set_parent(this);
}

const SigRole& RoleBinding::get_role_def()
{
	assert(m_parent);
	return Network::get(m_parent->get_type())->get_sig_role(m_id);
}

std::string RoleBinding::to_string()
{
	// Print parent port, role name, and tag, for nicer error messages
	assert(m_parent);
	std::string result = m_parent->get_hier_path();
			
	// Add binding
	result += "::";
	if (m_binding)
		result += m_binding->to_string();
	else
		result += "(no binding)";
			
	// Add role name and tag
	auto rdef = get_role_def();
	result += " (" + rdef.get_name();
	if (rdef.get_uses_tags())
	{
		result += " " + m_tag;
	}
	result += ")";

	return result;
}