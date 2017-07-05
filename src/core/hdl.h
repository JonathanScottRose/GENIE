#pragma once

#include <string>
#include "int_expr.h"
#include "params.h"
#include "prop_macros.h"
#include "genie/port.h"
#include "genie/smart_enum.h"

namespace genie
{
namespace impl
{
    class Node;

namespace hdl
{
	class HDLState;
	class Port;
	class PortBinding;
    class PortBindingRef;
	class PortBindingTarget;
	class Net;

	class PortBindingTarget
	{
	public:
		bool is_const() const;
		int get_width(const HDLState&) const;
		int get_depth(const HDLState&) const;

		const BitsVal& get_const() const;
		const std::string& get_net_name() const;

		PortBindingTarget();
		PortBindingTarget(const BitsVal&);
		PortBindingTarget(const std::string&);
		
		PortBindingTarget(const PortBindingTarget&);

		PortBindingTarget& operator=(const PortBindingTarget&);
		// TODO: 'move' versions, if performance becomes a problem

		~PortBindingTarget();

	private:
		enum { UNSET, NET, CONST } m_type;
		union
		{
			BitsVal m_const;
			std::string m_net;
		};
	};

	class PortBinding
	{
	public:
		PortBinding(Port* parent, const PortBindingTarget& target);

		PROP_GET_SET(target, const PortBindingTarget&, m_target);
		PROP_GET_SET(parent, Port*, m_parent);
		PROP_GET_SET(target_lo_slice, int, m_target_lo_slice);
        PROP_GET_SET(target_lo_bit, int, m_target_lo_bit);
        PROP_GET_SET(port_lo_slice, int, m_port_lo_slice);
        PROP_GET_SET(port_lo_bit, int, m_port_lo_bit);
        PROP_GET_SET(bound_slices, int, m_bound_slices);
        PROP_GET_SET(bound_bits, int, m_bound_bits);

		// Does the binding bind to the (port/target)'s full 2D size?
        bool is_full1_target_binding(const HDLState&) const;
        bool is_full1_port_binding() const;

        // Does the binding bind to an entire 1D slice of the (port/target)?
        bool is_full0_target_binding(const HDLState&) const;	
		bool is_full0_port_binding() const;	

	protected:
		Port* m_parent;
		PortBindingTarget m_target;
        int m_target_lo_slice;
        int m_target_lo_bit;
        int m_port_lo_slice;
        int m_port_lo_bit;
        int m_bound_slices;
        int m_bound_bits;
	};

    class PortBindingRef
    {
    public:
        PortBindingRef();
        ~PortBindingRef() = default;
        PortBindingRef(const PortBindingRef&) = default;
        PortBindingRef(PortBindingRef&&) = default;
        PortBindingRef& operator= (const PortBindingRef&) = default;
        PortBindingRef& operator= (PortBindingRef&&) = default;

		// 1-bit, n-bit 1-slice, n-bit m-slice
		PortBindingRef(const std::string& name);
		PortBindingRef(const std::string& name, const IntExpr& bits);
		PortBindingRef(const std::string& name, const IntExpr& bits, const IntExpr& slices);

		PortBindingRef(const char* name);
		PortBindingRef(const char* name, const IntExpr& bits);
		PortBindingRef(const char* name, const IntExpr& bits, const IntExpr& slices);
		
        void resolve_params(ParamResolver&);
        bool is_resolved() const;
        std::string to_string() const;

        PROP_GET_SET(port_name, const std::string&, m_port_name);
        PROP_GET_SET(slices, const IntExpr&, m_slices);
        PROP_GET(lo_slice, const IntExpr&, m_lo_slice);
        PROP_GET_SET(bits, const IntExpr&, m_bits);
        PROP_GET(lo_bit, const IntExpr&, m_lo_bit);
		PortBindingRef& set_lo_bit(const IntExpr&);
		PortBindingRef& set_lo_slice(const IntExpr&);

    protected:
        std::string m_port_name;
        IntExpr m_slices;
        IntExpr m_lo_slice;
        IntExpr m_bits;
        IntExpr m_lo_bit;
    };

	class Port
	{
	public:
        using Bindings = std::vector<PortBinding>;

		SMART_ENUM_EX(Dir,
		(
		public:
			Dir rev() const;
			static Dir from_logical(genie::Port::Dir);
		), IN, OUT, INOUT);

		Port(const std::string& m_name);
		Port(const std::string& m_name, const IntExpr& width, const IntExpr& depth, Dir dir);
		Port(const Port&);
        Port(Port&&);
		~Port();

		PROP_GET_SET(name, const std::string&, m_name);
		PROP_GET_SET(dir, Dir, m_dir);
		PROP_GET_SET(parent, HDLState*, m_parent);
		PROP_GET_SET(width, const IntExpr&, m_width);
        PROP_GET_SET(depth, const IntExpr&, m_depth);

		bool sizes_are_resolved();
        void resolve_params(ParamResolver&);
		
		const Bindings& bindings() const { return m_bindings; }
		bool is_bound() const;

        void bind(const PortBindingTarget&, unsigned bind_dim, int bind_size,
            int targ_slice, int targ_bit,
            int port_slice, int port_bit);

	protected:
		HDLState* m_parent;
		IntExpr m_width;
        IntExpr m_depth;
		std::string m_name;
		Dir m_dir;
		Bindings m_bindings;
	};

	class Net
	{
	public:
		enum Type
		{
			WIRE,
			EXPORT
		};

		Net(Type type, const std::string& name);

		PROP_GET_SET(type, Type, m_type);
		PROP_GET(name, const std::string&, m_name);
		int get_width() const;
		int get_depth() const;

		void set_width(int width);
        void set_depth(int depth);

	protected:
		std::string m_name;
		Type m_type;
		int m_width;
        int m_depth;
	};

    class HDLState
	{
    protected:
        Node* m_node;
        std::unordered_map<std::string, Port> m_ports;
        std::unordered_map<std::string, Net> m_nets;

        Net& add_net(Net::Type type, const std::string& name);
		void update_parent_refs();

	public:
        HDLState(Node*);
        HDLState(const HDLState&);
		HDLState(HDLState&&);
		~HDLState();

		HDLState& operator= (const HDLState&);
		HDLState& operator= (HDLState&&);

        void resolve_params(ParamResolver&);
		void reintegrate(HDLState&&);
        PROP_GET_SET(node, Node*, m_node);

        Port& get_or_create_port(const std::string& name, const IntExpr& width, 
            const IntExpr& depth, Port::Dir dir);

        Port& add_port(const std::string& name);
        Port& add_port(const std::string& name, const IntExpr& width, 
            const IntExpr& depth, Port::Dir dir);
        Port* get_port(const std::string&);
        const decltype(m_ports)& get_ports() const;

		Net* get_net(const std::string&) const;
        const decltype(m_nets)& get_nets() const;

        void connect(Port* src, Port* sink, int src_slice, int src_lsb,
            int sink_slice, int sink_lsb, unsigned dim, int size);
        void connect(Port*, const BitsVal&, int sink_slice, int sink_lsb);
        void connect(Node* src_node, const PortBindingRef& src, Node* sink_node,
			const PortBindingRef& sink);
        void connect(Node*, const PortBindingRef& sink, const BitsVal&);
	};
}
}
}