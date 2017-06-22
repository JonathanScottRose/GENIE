#pragma once

#include "genie/genie.h"
#include "genie/smart_enum.h"

namespace genie
{
    class Node;

    // Properties of an HDL Port
    struct HDLPortSpec
    {
        std::string name;
        std::string width;
        std::string depth;
    };

    // A binding to (part of) an HDL port
    struct HDLBindSpec
    {
        std::string lo_bit;
        std::string width;
        std::string lo_slice;
        std::string depth;
    };

	// Signal Role Type
	struct SigRoleType
	{
		// Constant to indicate unknown/invalid signal role type
		static const SigRoleType INVALID;

		static SigRoleType from_string(const std::string&);
		const std::string& to_string() const;
	
		SigRoleType();
		SigRoleType(unsigned);
		bool operator< (const SigRoleType&) const;
		bool operator== (const SigRoleType&) const;
		bool operator!= (const SigRoleType&) const;
		explicit operator unsigned() const;

	private:
		unsigned _val;
	};

	// Signal Role ID (type+tag)
	// Uniquely identifies an instance of a signal role ID within a Port
	struct SigRoleID
	{
		SigRoleID();
		SigRoleID(SigRoleType);
		SigRoleID(SigRoleType, const std::string&);

		bool operator< (const SigRoleID&) const;
		bool operator== (const SigRoleID&) const;
		bool operator!= (const SigRoleID&) const;

		SigRoleType type;
		std::string tag;
	};

	// Port base class
    class Port : virtual public HierObject
    {
    public:
		SMART_ENUM_EX(Dir, 
		(
			public:
				Dir rev() const;
		), IN, OUT);

        virtual Dir get_dir() const = 0;
		virtual void add_signal(const SigRoleID& role,
			const std::string& sig_name, const std::string& width) = 0;
		virtual void add_signal_ex(const SigRoleID& role,
			const HDLPortSpec&, const HDLBindSpec&) = 0;

    protected:
        virtual ~Port() = default;
    };

    class PortConduit : virtual public Port
    {
    public:
		static SigRoleType FWD, REV, IN, OUT, INOUT;

	protected:
		virtual ~PortConduit() = default;
    };

    class PortRS : virtual public Port
    {
    public:
		static SigRoleType VALID, READY, DATA, DATABUNDLE, EOP, ADDRESS;

		virtual void set_logic_depth(unsigned) = 0;
		virtual void set_default_packet_size(unsigned) = 0;
		virtual void set_default_importance(float) = 0;

	protected:
		virtual ~PortRS() = default;
    };
}