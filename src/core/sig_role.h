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
	// Direction of signal with respect to parent port.
	enum Sense
	{
		FWD,
		REV,
		IN,
		OUT,
		INOUT
	};

	SigRoleDef(const std::string&, bool uses_tags, Sense sense = FWD);
	
	PROP_GET(name, const std::string&, m_name);
	PROP_GET(sense, Sense, m_sense);
	bool uses_tags() const { return m_uses_tags; }

protected:
	std::string m_name;
	bool m_uses_tags;
	Sense m_sense;
};

}
}
