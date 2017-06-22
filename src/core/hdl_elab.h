#pragma once

namespace genie
{
namespace impl
{
	class NodeSystem;
	class Port;

	namespace hdl
	{
		void connect_ports(NodeSystem* sys, impl::Port* src, impl::Port* sink);
		void elab_system(NodeSystem*);
	}
}
}