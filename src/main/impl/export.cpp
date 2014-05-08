#include "impl_vlog.h"
#include "ct/export_node.h"

using namespace ct;
using namespace ImplVerilog;

namespace
{
	// copied from instance.cpp. please kill me now
	Vlog::Port::Dir conv_port_dir(Spec::Interface::Type itype, Spec::Signal::Sense ssense)
	{
		using namespace ct::Spec;

		bool is_out = ssense == Signal::FWD;

		switch (itype)
		{
			case Interface::CLOCK_SINK:
			case Interface::RESET_SINK:
			case Interface::RECV:
				is_out = !is_out;
				break;
		}

		return is_out ? Vlog::Port::OUT : Vlog::Port::IN;
	}

	class : public ImplVerilog::INodeImpl
	{
		// Visiting a (the) export node adds top-level ports and nets to
		// the system module
		void visit(INetlist* netlist, P2P::Node* generic_node)
		{
			auto node = (P2P::ExportNode*)generic_node;
			assert(node->get_type() == P2P::Node::EXPORT);

			Vlog::SystemModule* sysmod = netlist->get_system_module();

			// add ports according to the Spec::Component for this system, which has the
			// same name as the system
			Spec::Component* comp_def = Spec::get_component(node->get_name());

			// Convert ports
			for (auto& i : comp_def->interfaces())
			{
				Spec::Interface* iface = i.second;
				const std::string& ifacename = iface->get_name();

				for (Spec::Signal* sig : iface->signals())
				{
					std::string sig_name = sig->get_aspect_val<std::string>();

					// Create port. The direction of the Interface actually points the right way.
					Vlog::Port* port = new Vlog::Port(sig_name, sysmod);
					port->set_dir(conv_port_dir(iface->get_type(), sig->get_sense()));
					port->width() = sig->get_width(); // is a constant
					sysmod->add_port(port);

					// Create corresponding export net
					auto net = new Vlog::ExportNet(port);
					sysmod->add_net(net);
				}
			}
		}

		// An export node's ports correspond to the system module's top-level ports, and already
		// have nets associated with each input and output. 
		Vlog::Net* produce_net(INetlist* netlist, P2P::Node* generic_node, P2P::Port* port, 
			P2P::Field* field, int* net_lo)
		{
			Spec::Component* comp_def = Spec::get_component(generic_node->get_name());
			Spec::Interface* iface = comp_def->get_interface(port->get_name());
			Spec::Signal* sig = iface->get_signal(field->name);
			std::string sig_name = sig->get_aspect_val<std::string>();
			if (net_lo) *net_lo = 0;
			return netlist->get_system_module()->get_net(sig_name);
		}

		// No kind of binding is required
		void accept_net(INetlist* netlist, P2P::Node* generic_node, P2P::Port* port, 
			P2P::Field* field, Vlog::Bindable* net, int net_lo)
		{
		}
	} s_impl;

	ImplVerilog::BuiltinReg s_impl_reg([]
	{
		ImplVerilog::register_impl(P2P::Node::EXPORT, &s_impl);
	});
}
