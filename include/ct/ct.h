#pragma once

#include "ct/hierarchy.h"
#include "ct/networks.h"
#include "ct/structure.h"

namespace ct
{
	template<class T, class O>
	T as_a(O ptr)
	{
		T result = dynamic_cast<T>(ptr);
		if (!result)
		{
			throw Exception("Failed casting " + 
				std::string(typeid(O).name()) + " to " + 
				std::string(typeid(T).name()));
		}
		return result;
	}

	template<class T, class O>
	bool is_a(O ptr)
	{
		return dynamic_cast<T>(ptr) != nullptr;
	}

	class Registry : public HierContainer
	{
	public:
		Registry();
		~Registry();

		void reg_net_def(NetworkDef*);
		NetworkDef* get_net_def(const NetworkID&) const;

		void reg_system(System*);
		System* get_system(const std::string&) const;
		List<System*> get_systems() const;

		void reg_node_def(NodeDef*);
		NodeDef* get_node_def(const std::string&) const;
		List<NodeDef*> get_node_defs() const;

		const std::string& hier_name() const;
		HierNode* hier_child(const std::string&) const;
		List<HierNode*> hier_children() const;
		HierNode* hier_parent() const;
	};

	Registry* get_root();
}