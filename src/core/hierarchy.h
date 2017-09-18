#pragma once

#include <string>
#include <functional>
#include "network.h"
#include "genie/port.h"
#include "genie/genie.h"

namespace genie
{
namespace impl
{
	class HierObject;

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
    
	class HierObject : virtual public genie::HierObject
	{
	public:
		const std::string& get_name() const override;
		std::string get_hier_path(const genie::HierObject* rel_to = nullptr) const override;
		genie::HierObject* get_child(const std::string&) const override;

	public:
		static const char PATH_SEP; // must create in .cpp file because GCC sucks

        // Used by the filtered version of get_children() to choose specific children
        // that satisfy some criteria.
		template<class T = HierObject>
		using FilterFunc = std::function<bool(const T*)>;

		HierObject();
		HierObject(const HierObject&);
		virtual ~HierObject();
		virtual HierObject* clone() const = 0;

		virtual void reintegrate(HierObject*) {}

		// Name
		void set_name(const std::string&);

		// Parent
		HierObject* get_parent() const;
		bool is_parent_of(const HierObject*) const;
		template<class T>
		T* get_parent_by_type() const
		{
			T* result = nullptr;
			for (HierObject* cur = get_parent();
				cur && !(result = dynamic_cast<T*>(cur));
				cur = cur->get_parent());

			return result;
		}

		// Add a child object
		void add_child(HierObject*);

		// Get or query existence of an existing child, by hierarchical path.
		bool has_child(const std::string&) const;
		template<class T>
		T* get_child_as(const std::string& p) const
		{
			return dynamic_cast<T*>(get_child(p));
		}

		// Remove child but do not destroy (returns removed thing)
		HierObject* remove_child(const std::string&);
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
		Container get_children(const FilterFunc<T>& filter) const
		{
			Container result;
			for (auto child : m_children)
			{
				auto oo = dynamic_cast<T*>(child.second);
				if (oo && filter(oo)) result.push_back(oo);
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

        // Return a unique child name
        std::string make_unique_child_name(const std::string& base);

		// Connectivity
		void make_connectable(NetType);
		void make_connectable(NetType, genie::Port::Dir);
		bool is_connectable(NetType, genie::Port::Dir) const;
		Endpoint* get_endpoint(NetType, genie::Port::Dir) const;

	protected:
		void set_parent(HierObject*);

	private:
		std::string m_name;
		HierObject* m_parent;
		std::unordered_map <std::string, HierObject*> m_children;
		std::unordered_map<NetType, EndpointPair> m_endpoints;
	};

	class IInstantiable
	{
	public:
		virtual HierObject* instantiate() const = 0;

	protected:
		~IInstantiable() = default;
	};
}
}