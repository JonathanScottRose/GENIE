#pragma once

#include "impl_vlog.h"
#include "ct/export_node.h"

using namespace ct;
using namespace ImplVerilog;

namespace
{
	class : public ImplVerilog::INodeImpl
	{
		// Visiting a (the) export node adds top-level ports and nets to
		// the system module
		void visit(INetlist* netlist, P2P::Node* generic_node)
		{
			auto node = (P2P::ExportNode*)generic_node;
			assert(node->get_type() == P2P::Node::EXPORT);

			Vlog::SystemModule* sysmod = netlist->get_system_module();

			for (auto& i : node->ports())
			{
				P2P::Port* p = i.second;

				for (P2P::Field* f : p->get_proto().fields())
				{
					// Create port. Don't forget to reverse the direction of the port, because
					// the export node's ports point inwards towards the system components but
					// we want to create a port facing outwards to a higher-level module.
					std::string portname = ImplVerilog::name_for_p2p_port(p, f);
					Vlog::Port* port = new Vlog::Port(portname, sysmod);
					port->set_dir(Vlog::Port::rev_dir(ImplVerilog::conv_port_dir(p, f))); // reverse dir!
					port->set_width(f->width);
					sysmod->add_port(port);

					// Create corresponding export net
					auto net = new Vlog::ExportNet(port);
					sysmod->add_net(net);
				}
			}
		}

		// An export node's ports correspond to the system module's top-level ports, and already
		// have nets associated with each input and output. Return the name of these nets
		// and return true.
		Vlog::Net* produce_net(INetlist* netlist, P2P::Node* generic_node, P2P::Port* port, 
			P2P::Field* field)
		{
			std::string name = ImplVerilog::name_for_p2p_port(port, field);
			return netlist->get_system_module()->get_net(name);
		}

		// No kind of binding is required
		void accept_net(INetlist* netlist, P2P::Node* generic_node, P2P::Port* port, P2P::Field* field,
			Vlog::Bindable* net)
		{
		}
	} s_impl;

	ImplVerilog::BuiltinReg s_impl_reg([]
	{
		ImplVerilog::register_impl(P2P::Node::EXPORT, &s_impl);
	});
}