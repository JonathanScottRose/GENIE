#include "genie/sigroles.h"
#include "genie/networks.h"
#include "genie/structure.h"

using namespace genie;

namespace
{
    SigRoleID s_next_id = 0;

    List<SigRole>& s_get_all_roles()
    {
        static List<SigRole> all_roles;
        return all_roles;
    }
}

//
// SigRole
//

void SigRole::init()
{
    s_get_all_roles().clear();    
    s_next_id = 0;
}

SigRoleID SigRole::reg(const std::string& name, Sense sense, bool tags)
{
    SigRoleID id = s_next_id++;
	s_get_all_roles().push_back(SigRole(id, name, sense, tags));

	return id;
}

const SigRole* SigRole::get(SigRoleID id)
{
	if (id > s_get_all_roles().size())
		return nullptr;

	return &s_get_all_roles()[id];
}

SigRole::SigRole(SigRoleID id, const std::string& name, Sense sense, bool tags)
	: m_id(id), m_name(util::str_tolower(name)), m_sense(sense), m_uses_tags(tags)
{
}

//
// RoleBinding
//

RoleBinding::RoleBinding(SigRoleID id, const std::string& tag, HDLBinding* hdl)
	: m_id(id), m_tag(tag), m_parent(nullptr), m_binding(nullptr)
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

const SigRole* RoleBinding::get_role_def() const
{
	return SigRole::get(m_id);
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
	result += " (" + rdef->get_name();
	if (rdef->get_uses_tags())
	{
		result += " " + m_tag;
	}
	result += ")";

	return result;
}

bool RoleBinding::has_tag() const
{
	return !m_tag.empty();
}

SigRole::Sense SigRole::rev_sense(SigRole::Sense other)
{
	Sense result;

	switch(other)
	{
	case SigRole::FWD: result = SigRole::REV; break;
	case SigRole::REV: result = SigRole::FWD; break;
	case SigRole::IN: result = SigRole::OUT; break;
	case SigRole::OUT: result = SigRole::IN; break;
	case SigRole::INOUT: result = SigRole::INOUT; break;
	default: assert(false);
	}

	return result;
}

SigRole::Sense RoleBinding::get_absolute_sense()
{
	SigRole::Sense sense = this->get_role_def()->get_sense();

	// Step 1: if sense is FWD/REV, combine with port direction to get an IN/OUT
	bool flip = this->get_parent()->get_dir() == Dir::IN;
	switch(sense)
	{
	case SigRole::FWD: sense = flip? SigRole::IN : SigRole::OUT; break;
	case SigRole::REV: sense = flip? SigRole::OUT : SigRole::IN; break;
	}

	// Step 2: flip again if we're an export
	if (this->get_parent()->is_export())
		sense = SigRole::rev_sense(sense);
	
	return sense;
}