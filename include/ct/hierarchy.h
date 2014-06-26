#pragma once

#include "common.h"

namespace ct
{
	class HierNode;
	class HierContainer;

	typedef std::string HierPath;

	// This is an interface that allows a class to participate in The Hierarchy.
	//
	// A HierNode must be able to name itself,
	// return its parent (can be null), return a direct child by name(can be null if not found)
	// and return a list of all children. All returned nodes are generic HierNodes and may need 
	// back into a specific type.
	class HierNode
	{
	public:
		// Used as result of get_children()
		typedef List<HierNode*> Children;

		// A function that acceps a HierNode and decides whether or not to include
		// it in a list of children when calling the filtered version of get_children()
		typedef std::function<bool(HierNode*)> FilterFunc;
		
		// Pure virtual interface methods that each HierNode must implement, as described above.
		// Overriding hier_child/hier_children is optional. If not, the class acts like a leaf node.
		virtual const std::string& hier_name() const = 0;
		virtual HierNode* hier_parent() const = 0;
		virtual HierNode* hier_child(const std::string&) const { return nullptr; }
		virtual Children hier_children() const { return Children(); }

		// Returns only the children satisfying the given filter function
		Children hier_children(const FilterFunc& filter) const;

		// Convenience function.
		// Given a class T, return all children that can be cast into T, as a nice vector<T*>
		// (note: must be done element by element due to possible pointer adjustment by dynamic_cast)
		template<class T>
		List<T> hier_children_as() const
		{
			List<T> result;
			Children allchildren = hier_children();
			for (HierNode* child : allchildren)
			{
				T casted = dynamic_cast<T>(child);
				if (casted) result.push_back(casted);
			}
			return result;
		}

		template<class T>
		T hier_child_as(const std::string& name) const
		{
			return as_a<T>(hier_child(name));
		}

		// Returns the node's full hierarchical name with respect to the root
		HierPath hier_full_name() const;

		// Get a descendent (not necessarily a direct child) by relative path to this node
		HierNode* hier_node(const HierPath&) const;
	};

	//
	// A concrete class that is a generic container of HierNodes.
	//
	class HierContainer : public HierNode
	{
	public:
		HierContainer(const std::string& name = std::string(), HierNode* parent = nullptr);
		~HierContainer();

		virtual const std::string& hier_name() const;
		virtual HierNode* hier_parent() const;
		virtual HierNode* hier_child(const std::string&) const;
		virtual Children hier_children() const;

		virtual void hier_add(HierNode* node);

		PROP_GET_SET(name, const std::string&, m_name);
		PROP_SET(parent, HierNode*, m_parent);

	protected:
		HierNode* m_parent;
		std::string m_name;
		StringMap<HierNode*> m_nodes;
	};
}