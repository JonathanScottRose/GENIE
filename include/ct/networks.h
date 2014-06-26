#pragma once

#include "ct/common.h"
#include "ct/hierarchy.h"

namespace ct
{
	// Use this to refer to a network type everywhere else in CT
	typedef std::string NetworkID;

	// Network types derive from this.
	class NetworkDef : public HierNode
	{
	public:
		static const std::string ID; // category ID

		NetworkDef(const NetworkID&);
		virtual ~NetworkDef() { }
		
		const NetworkID& id() const;
		const std::string& get_descriptive_name() const; // descriptive name
		bool allow_src_multibind() const;
		bool allow_sink_multibind() const;
		bool allow_conn_multibind() const;

		const std::string& hier_name() const;
		HierNode* hier_parent() const;

	protected:
		const NetworkID& m_id;
		std::string m_name;
		bool m_src_multibind;
		bool m_sink_multibind;
		bool m_conn_multibind;
	};

	
}
