#pragma once

#include "genie/vlog.h"
#include "genie/structure.h"

namespace genie
{
namespace vlog
{
	// Verilog implementation of HDLBinding - binds a signal role to a Vlog Port
	class VlogBinding : public HDLBinding
	{
	public:
		virtual int get_lsb() const = 0;
		virtual Port* get_port() const = 0;
	};

	// Regular, static verilog binding that can be parameterized
	class VlogStaticBinding : public VlogBinding
	{
	public:
		// Empty, default
		VlogStaticBinding();
		// Binds to entirety of provided port
		VlogStaticBinding(const std::string&);
		// Binds to [width-1:0] of provided port
		VlogStaticBinding(const std::string&, const Expression&);
		// Full control
		VlogStaticBinding(const std::string&, const Expression&, const Expression&);

		PROP_GET_SET(port_name, const std::string&, m_port_name);
		void set_lsb(const Expression&);
		void set_width(const Expression&);

		HDLBinding* clone() override;
		int get_lsb() const override;
		int get_width() const override;
		Port* get_port() const override;
		std::string to_string() const override;

	protected:
		bool m_full_width;
		std::string m_port_name;
		Expression m_lsb;
		Expression m_width;
	};
}
}