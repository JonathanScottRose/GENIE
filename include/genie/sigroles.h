#pragma once

#include <memory>
#include "genie/common.h"

namespace genie
{
	class HierObject;
	class Port;
	class RoleBinding;
	class HDLBinding;

	typedef unsigned int SigRoleID;
	const SigRoleID ROLE_INVALID = std::numeric_limits<SigRoleID>::max();

	class SigRole
	{
	public:
		enum Sense
		{
			// Go with Port direction
			FWD,
			REV,
			// Absolute, ignoring port direction
			IN,
			OUT,
			INOUT
		};

		SigRole(const std::string& name, Sense sense, bool tags = false)
			: m_name(name), m_sense(sense), m_uses_tags(tags), m_id(ROLE_INVALID) {}

		PROP_GET_SET(name, const std::string&, m_name);
		PROP_GET_SET(id, SigRoleID, m_id);
		PROP_GET_SET(uses_tags, bool, m_uses_tags);
		PROP_GET_SET(sense, Sense, m_sense);

	protected:
		std::string m_name;
		SigRoleID m_id;
		bool m_uses_tags;
		Sense m_sense;
	};

	class HDLBinding
	{
	public:
		virtual ~HDLBinding() = 0 { };
		virtual HDLBinding* clone() = 0;
		virtual int get_width() const = 0;
		virtual std::string to_string() const = 0;
		virtual HDLBinding* export_pre() = 0;
		virtual void export_post() = 0;

		PROP_GET_SET(parent, RoleBinding*, m_parent);

	protected:
		RoleBinding* m_parent;
	};

	class RoleBinding
	{
	public:
		RoleBinding(SigRoleID, const std::string&, HDLBinding*);
		RoleBinding(SigRoleID, HDLBinding*);
		RoleBinding(const RoleBinding&);
		~RoleBinding();

		PROP_GET_SET(id, SigRoleID, m_id);
		PROP_GET_SET(tag, std::string, m_tag);
		PROP_GET(hdl_binding, HDLBinding*, m_binding);
		PROP_GET_SET(parent, Port*, m_parent);

		void set_hdl_binding(HDLBinding* b);
		const SigRole& get_role_def();
		std::string to_string();

	protected:
		Port* m_parent;
		SigRoleID m_id = ROLE_INVALID;
		std::string m_tag = "";
		HDLBinding* m_binding = nullptr;
	};
}