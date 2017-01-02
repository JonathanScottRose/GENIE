#pragma once

#include <string>
#include <functional>
#include "genie/genie.h"

namespace genie
{
namespace impl
{
	class HierObject;

	// Represents a hierarchy path. Just a string for now.
	typedef std::string HierPath;

	// Global namespace functions.

	// Make reserved name
	//std::string hier_make_reserved_name(const std::string&);

	// Combine two paths together (TODO: make HierPath a class)
	//HierPath hier_path_append(const HierPath&, const HierPath&);

	// Create a single name with hierarchy separators replaced by underscores
	//std::string hier_path_collapse(const HierPath&);

    // Exceptions
	class HierDupException : public Exception
	{
		const HierObject* m_target;
		const HierObject* m_parent;
	public:
		const HierObject* get_target() const { return m_target; }
		const HierObject* get_parent() const { return m_parent; }
		HierDupException(const HierObject* parent, const HierObject* target);
	};
    
	class HierObject
	{
	public:
		static const char PATH_SEP = '.';

		// Used by the filtered version of get_children() to choose specific children
		// that satisfy some criteria.
		typedef std::function<bool(const HierObject*)> FilterFunc;

		HierObject();
		HierObject(const HierObject&);
		virtual ~HierObject();

		// Name
		virtual const std::string& get_name() const;
		virtual void set_name(const std::string&);

		// Gets hierarchy path, relative to another parent (default is root)
		HierPath get_hier_path(const HierObject* rel_to = nullptr) const;

		// Parent
		HierObject* get_parent() const;

		// Add a child object
		void add_child(HierObject*);

		// Get or query existence of an existing child, by hierarchical path.
		HierObject* get_child(const HierPath&) const;
		bool has_child(const HierPath&) const;
		template<class T>
		T* get_child_as(const HierPath& p) const
		{
			return util::as_a<T>(get_child(p));
		}

		// Remove child but do not destroy (returns removed thing)
		HierObject* remove_child(const HierPath&);
		HierObject* remove_child(HierObject* c);
		void unlink_from_parent();

		// Get all children as HierObjects put into whatever container you like
		template<class Container = std::vector<HierObject*>>
		Container get_children() const
		{
			return util::values<Container>(m_children);
		}

		// Get children satisfying a filter condition
		template<class T = HierObject, class Container = std::vector<T*>>
		Container get_children(const FilterFunc& filter) const
		{
			Container result;
			for (const auto& child : m_children)
			{
				if (filter(child.second)) result.push_back(util::as_a<T*>(child.second));
			}
			return result;
		}

		// Convenience function to get children that are castable to a specific type
		template<class T, class Container = std::vector<T*>>
		Container get_children_by_type() const
		{
			Container result;
			for (const auto& child : m_children)
			{
				T* casted = util::as_a<T>(child.second);
				if (casted) result.push_back(casted);
			}
			return result;
		}

		// Convenience function to get children that implement a particular Aspect
	/*	template<class A, class T>
		List<T*> get_children_with_aspect() const
		{
			return get_children([] (const HierObject* obj)
			{
				return obj->asp_has<A>();
			});
		}*/

        // Return a unique child name
        std::string make_unique_child_name(const std::string& base);

	protected:
		void set_parent(HierObject*);

	private:
		std::string m_name;
		HierObject* m_parent;
		std::unordered_map <std::string, HierObject*> m_children;
	};
}
}