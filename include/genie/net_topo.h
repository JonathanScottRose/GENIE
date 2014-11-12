#pragma once

#include "genie/networks.h"
#include "genie/structure.h"

namespace genie
{
	extern const NetType NET_TOPO;

	class ATopoEndpoint : public AEndpoint
	{
	public:
		ATopoEndpoint(Dir, HierObject*);
		~ATopoEndpoint();

		NetType get_type() const { return NET_TOPO;	}
	};

	typedef Link TopoLink;
}