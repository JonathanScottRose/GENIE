#include "pch.h"
#include "sig_role.h"
#include "genie_priv.h"
#include "genie/port.h"

using namespace genie::impl;

const SigRoleType SigRoleType::INVALID = std::numeric_limits<unsigned>::max();

//
// SigRoleType - for internal registration
//

SigRoleDef::SigRoleDef(const std::string& name, bool uses_tags, Sense sense)
	: m_name(name), m_uses_tags(uses_tags), m_sense(sense)
{
}

//
// genie::SigRoleType - public, identifies a role type
//

SigRoleType::SigRoleType()
{
	// Leave uninitialized
}

SigRoleType::SigRoleType(unsigned val)
	: _val(val)
{
}

bool SigRoleType::operator < (const SigRoleType& that) const
{
	return _val < that._val;
}

bool SigRoleType::operator==(const SigRoleType &that) const
{
	return _val == that._val;
}

bool SigRoleType::operator!=(const SigRoleType &that) const
{
	return !(*this == that);
}

SigRoleType::operator unsigned() const
{
	return _val;
}

SigRoleType SigRoleType::from_string(const std::string& str)
{
	return genie::impl::get_sig_role_from_str(str);
}

const std::string& SigRoleType::to_string() const
{
	auto def = genie::impl::get_sig_role(*this);
	if (!def)
		throw Exception("invalid signal role");

	return def->get_name();
}

//
// genie::SigRoleID - public, identifies a unique instance of a role within a Port
//

SigRoleID::SigRoleID()
{
	// default, leave uninitialized
}

SigRoleID::SigRoleID(SigRoleType type_)
	: type(type_)
{
}

SigRoleID::SigRoleID(SigRoleType type_, const std::string& tag_)
	: type(type_), tag(tag_)
{
}

bool SigRoleID::operator<(const SigRoleID &that) const
{
	return type < that.type ||
		(type == that.type && tag < that.tag);
}

bool SigRoleID::operator==(const SigRoleID &that) const
{
	return type == that.type && tag == that.tag;
}

bool SigRoleID::operator!=(const SigRoleID &that) const
{
	return !(*this == that);
}


