#pragma once

#include "genie/vlog.h"
#include "genie/structure.h"

namespace genie
{
namespace vlog
{
	// Attached to a Node
	class AVlogInfo : public AspectWithRef<Node>
	{
	public:
		AVlogInfo();
		~AVlogInfo();

		PROP_GET_SET(module, Module*, m_module);
		PROP_GET_SET(instance, Instance*, m_instance);

	protected:
		Aspect* asp_instantiate() override;

		Module* m_module;
		Instance* m_instance;
	};

	// Verilog implementation of HDLBinding - binds a signal role to a Vlog Port
	class VlogBinding : public HDLBinding
	{
	public:
		virtual int get_lo() const = 0;
		virtual Port* get_port() const = 0;
	};

	// Regular, static verilog binding that can be parameterized
	class VlogStaticBinding : public VlogBinding
	{
	public:
		// Empty, default
		VlogStaticBinding();
		// Binds to entirety of provided port
		VlogStaticBinding(Port*);
		// Binds to [width-1:0] of provided port
		VlogStaticBinding(Port*, const Expression&);
		// Full control
		VlogStaticBinding(Port*, const Expression&, const Expression&);

		PROP_SET(port, Port*, m_port);
		PROP_SET(lo, const Expression&, m_lo);
		PROP_SET(width, const Expression&, m_width);

		HDLBinding* clone() override;
		int get_lo() const override;
		int get_width() const override;
		Port* get_port() const override;
		std::string to_string() const override;
		HDLBinding* export_pre() override;
		void export_post() override;

	protected:
		Port* m_port;
		Expression m_lo;
		Expression m_width;
	};
}
}