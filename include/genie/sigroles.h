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
        static void init();

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

		static SigRoleID reg(const std::string& name, Sense sense, bool tags = false);
		static const SigRole* get(SigRoleID);

		PROP_GET(name, const std::string&, m_name);
		PROP_GET(id, SigRoleID, m_id);
		PROP_GET(uses_tags, bool, m_uses_tags);
		PROP_GET(sense, Sense, m_sense);

		static Sense rev_sense(Sense);

	protected:
		SigRole(SigRoleID, const std::string&, Sense, bool);

		std::string m_name;
		SigRoleID m_id;
		bool m_uses_tags;
		Sense m_sense;
	};

	class HDLBinding
	{
	public:
		virtual ~HDLBinding() = default;
		virtual HDLBinding* clone() = 0;
		virtual int get_width() const = 0;
		virtual std::string to_string() const = 0;

		PROP_GET_SET(parent, RoleBinding*, m_parent);

	protected:
		RoleBinding* m_parent;
	};

	class RoleBinding
	{
	public:
		RoleBinding(SigRoleID, const std::string&, HDLBinding* = nullptr);
		RoleBinding(SigRoleID, HDLBinding* = nullptr);
		RoleBinding(const RoleBinding&);
		~RoleBinding();

		PROP_GET_SET(id, SigRoleID, m_id);
		PROP_GET_SET(tag, std::string, m_tag);
		PROP_GET(hdl_binding, HDLBinding*, m_binding);
		PROP_GET_SET(parent, Port*, m_parent);

		void set_hdl_binding(HDLBinding* b);
		const SigRole* get_role_def() const;
		std::string to_string();
		bool has_tag() const;

		SigRole::Sense get_absolute_sense();

	protected:
		Port* m_parent;
		SigRoleID m_id;
		std::string m_tag;
		HDLBinding* m_binding;
	};
}