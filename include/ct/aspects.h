#pragma once

#include <typeindex>
#include <unordered_set>
#include <cassert>
#include <memory>

//
// Core of the Entity-Component system in ct, which lets individual C++ objects have different
// behavior/data at runtime attached to them via Aspects.
//
// An aspect-enabled class derives from Object. Attachable aspects derive from Aspect.

namespace ct
{
	// Unique ID for each type. Use C++11 RTTI by default
	typedef std::type_index AspectID;

	// Forward decls
	class Aspect;
	class Object;

	// Base class for Aspects
	class Aspect
	{
	protected:
		virtual ~Aspect() = 0 { };
		
		AspectID asp_id() const
		{
			return std::type_index(typeid(*this));
		}

		friend class Object;
	};

	// Base class for all objects that have aspects
	class Object
	{
	public:
		Object() = default; // empty aspect map
		~Object() = default; // destroy shared ptrs in map -> decrement refcounts
		Object(const Object&) = default; // point to same aspect instances as in original
		
		Aspect* asp_get(AspectID id) const
		{
			auto where = m_aspects.find(id);
			return where == m_aspects.end()? nullptr : where->second.get();
		}

		template<class T> T* asp_get() const
		{
			return (T*)asp_get(asp_id<T>());
		}

		template<class T> void asp_add(T* asp)
		{
			auto id = asp_id<T>();
			assert(m_aspects.count(id) == 0);
			m_aspects[id] = std::shared_ptr<Aspect>(asp, asp_deleter);
		}

		template<class T> void asp_replace(T* asp)
		{
			// same as add but without the assert. 
			// replaced aspect gets refcount decreased and possibly gets deleted
			auto asp_base = (Aspect*)asp;
			m_aspects[asp_id<T>()] = std::shared_ptr<Aspect>(asp_base, asp_deleter);
		}
		
		template<class T> void asp_remove()
		{
			m_aspects.erase(asp_id<T>());
		}

		template<class T> bool asp_has() const
		{
			return m_aspects.count(asp_id<T>()) > 0;
		}
		
	private:
		typedef std::unordered_map<AspectID, std::shared_ptr<Aspect>> Aspects;
		Aspects m_aspects;

		static void asp_deleter(Aspect* asp)
		{
			delete asp;
		}

		template<class T> AspectID asp_id() const
		{
			return std::type_index(typeid(T));
		}
	};
}