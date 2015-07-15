#pragma once

#include "genie/common.h"
#include "genie/networks.h"

namespace genie
{
	class HierObject;
	class Port;

	// Represents a hierarchy path. Just a string for now.
	typedef std::string HierPath;

	// Global namespace functions.

	// Make reserved name
	std::string hier_make_reserved_name(const std::string&);

	// Combine two paths together (TODO: make HierPath a class)
	HierPath hier_path_append(const HierPath&, const HierPath&);

	// Create a single name with hierarchy separators replaced by underscores
	std::string hier_path_collapse(const HierPath&);

    // Exceptions
	class HierException : public Exception
	{
	public:
		HierException(const HierObject* obj, const std::string& what);
	};

	class HierNotFoundException : public HierException
	{
	public:
		HierNotFoundException(const HierObject* parent, const HierPath& path);
	};

	class HierDupException : public HierException
	{
	public:
		HierDupException(const HierObject* parent, const std::string& what);
	};
    
	// A concrete, aspect-enabled base class for all Hierarchy members.
	// Has a name, and optionally also a parent and prototype.
	// When an asp_get call to a HierObject fails, it tries to look up that
	// aspect in its prototype.
	class HierObject : public Object
	{
	public:
		// Used by the filtered version of get_children() to choose specific children
		// that satisfy some criteria.
		typedef std::function<bool(const HierObject*)> FilterFunc;

		HierObject();
		HierObject(const HierObject&);
		virtual ~HierObject();

		// Name
		virtual const std::string& get_name() const;
		virtual void set_name(const std::string&, bool allow_reserved = false);

		// Gets hierarchy path, relative to another parent (default is root)
		HierPath get_hier_path(const HierObject* rel_to = nullptr) const;

		// Parent
		virtual HierObject* get_parent() const;

		// Instantiation
		virtual HierObject* instantiate() = 0;

		// Network refinement
		virtual void refine(NetType target);

		// Connectivity
		virtual Port* locate_port(Dir dir, NetType type = NET_INVALID);

		// Add a child object
		virtual void add_child(HierObject*);

		// Get or query existence of an existing child, by hierarchical path.
		HierObject* get_child(const HierPath&) const;
		bool has_child(const HierPath&) const;
		template<class T>
		T* get_child_as(const HierPath& p) const
		{
			auto result = as_a<T*>(get_child(p));
			if (!result)
				throw HierNotFoundException(this, p);

			return result;
		}

		// Remove child but do not destroy (returns removed thing)
		virtual HierObject* remove_child(const HierPath&);

		// Get all children as HierObjects put into whatever container you like
		template<class Container = List<HierObject*>>
		Container get_children() const
		{
			return util::values<Container>(m_children);
		}

		// Get children satisfying a filter condition
		template<class T = HierObject, class Container = List<T*>>
		Container get_children(const FilterFunc& filter) const
		{
			Container result;
			for (const auto& child : m_children)
			{
				if (filter(child.second)) result.push_back(as_a<T*>(child.second));
			}
			return result;
		}

		// Convenience function to get children that are castable to a specific type
		template<class T, class Container = List<T*>>
		Container get_children_by_type() const
		{
			Container result;
			for (const auto& child : m_children)
			{
				T* casted = dynamic_cast<T*>(child.second);
				if (casted) result.push_back(casted);
			}
			return result;
		}

		// Convenience function to get children that implement a particular Aspect
		template<class A, class T>
		List<T*> get_children_with_aspect() const
		{
			return get_children([] (const HierObject* obj)
			{
				return obj->asp_has<A>();
			});
		}

	protected:
		void set_parent(HierObject*);

		std::string m_name;
		HierObject* m_parent;
		StringMap<HierObject*> m_children;
	};
}