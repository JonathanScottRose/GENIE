#pragma once

#include <string>
#include "prop_macros.h"
#include "genie_priv.h"

namespace genie
{
namespace impl
{

class SigRoleDef
{
public:
	SigRoleDef(const std::string&, bool uses_tags);
	
	PROP_GET(name, const std::string&, m_name);
	bool uses_tags() const { return m_uses_tags; }

protected:
	std::string m_name;
	bool m_uses_tags;
};

}
}
