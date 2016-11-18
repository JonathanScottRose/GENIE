#pragma once

#include "genie/hdl.h"
#include "genie/structure.h"

namespace genie
{
	// Regular, static verilog binding that can be parameterized
	class HDLBinding
	{
	public:
		// Empty, default
		HDLBinding();
		// Binds to entirety of provided port
		HDLBinding(const std::string&);
		// Binds to [width-1:0] of provided port
		HDLBinding(const std::string&, const Expression&);
		// Full control
		HDLBinding(const std::string&, const Expression&, const Expression&);

		PROP_GET_SET(port_name, const std::string&, m_port_name);
        PROP_GET_SET(parent, RoleBinding*, m_parent);
		void set_lsb(const Expression&);
		void set_width(const Expression&);

		HDLBinding* clone();
		int get_lsb() const;
		int get_width() const;
		hdl::Port* get_port() const;
		std::string to_string() const;

	protected:
        RoleBinding* m_parent;
		bool m_full_width;
		std::string m_port_name;
		Expression m_lsb;
		Expression m_width;
	};
}