#pragma once

#include "genie/common.h"

namespace genie
{
	class HierObject;
	class HierFolder;
	class HierRoot;
	class AHierParent;	

	// Represents a hierarchy path. Just a string for now.
	typedef std::string HierPath;

	// A concrete, aspect-enabled base class for all Hierarchy members.
	// Has a name, and optionally also a parent and prototype.
	// When an asp_get call to a HierObject fails, it tries to look up that
	// aspect in its prototype.
	class HierObject : public Object
	{
	public:
		HierObject();
		HierObject(const std::string& name);
		virtual ~HierObject() = default;
		HierObject(const HierObject&) = delete; // until we figure out how to do this properly

		// Name
		virtual const std::string& get_name() const;
		virtual void set_name(const std::string&);
		
		// Gets full hierarchical name of this object.
		std::string get_full_name() const;

		// Gets full hierarchical path as a HierPath
		HierPath get_full_path() const;

		// Parent
		virtual HierObject* get_parent() const;
		virtual void set_parent(HierObject*);

		// Prototype
		virtual HierObject* get_prototype() const;
		virtual void set_prototype(HierObject*);
		virtual HierObject* instantiate();

	protected:
		std::string m_name;
		HierObject* m_parent;
		HierObject* m_prototype;

		// Checks m_prototype for aspect if aspect not found here
		Aspect* asp_not_found_handler(const AspectID&) const;
	};

	// An Aspect that allows a HierObject to be a parent of other HierObjects in the Hierarchy.
	class AHierParent : public Aspect
	{
	public:
		AHierParent(HierObject* container);
		~AHierParent();

		// Used by the filtered version of get_children() to choose specific children
		// that satisfy some criteria.
		typedef std::function<bool(const HierObject*)> FilterFunc;

		// Add an object as as child (must implement HierChild)
		void add_child(HierObject*);

		// Get or query existence of an existing child, by hierarchical path.
		HierObject* get_child(const HierPath&) const;
		bool has_child(const HierPath&) const;

		// Remove child but do not destroy (return it too if you want to delete it)
		HierObject* remove_child(const HierPath&);

		// Get all children as HierObjects put into whatever container you like
		template<class Container = List<HierObject*>>
		Container get_children() const
		{
			return Util::values<Container>(m_children);
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
		StringMap<HierObject*> m_children;
		HierObject* m_container;
	};

	// A named container for other hierarchical objects
	class HierFolder : public HierObject
	{
	public:
		HierFolder();
		~HierFolder();
	};

	// Exceptions
	class HierException : public Exception
	{
	public:
		HierException(const HierObject* obj, const std::string& what)
			: Exception(obj->get_full_name() + ": " + what) { }
	};

	class HierNotFoundException : public HierException
	{
	public:
		HierNotFoundException(const HierObject* parent, const HierPath& path)
			: HierException(parent, "Hierarchy path " + path + " not found.") { }
	};

	class HierDupException : public HierException
	{
	public:
		HierDupException(const HierObject* parent, const std::string& what)
			: HierException(parent, what) { }
	};
}