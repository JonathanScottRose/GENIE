#pragma once

#include <typeindex>
#include <unordered_map>
#include <cassert>

//
// Core of the Entity-Component system, which lets C++ objects have different
// behavior/data at runtime attached to them via Aspects.
//
// An aspect-enabled class derives from Object. Attachable aspects derive from Aspect
//

namespace genie
{
	// Unique ID for each type. Use C++11 RTTI by default to implement this.
	typedef std::type_index AspectID;
	const AspectID ASPECT_NULL = typeid(nullptr);

	// Forward decls
	class Aspect;
	class Object;

	// Base class for regular Aspects: ones which don't need a pointer back to their Object and ones
	// whose instances are different and separate objects than the Object.
	class Aspect
	{
	public:
		template<class T>
		static AspectID asp_id_of()
		{
			return std::type_index(typeid(T));
		}

		// Returns the ID of this Aspect
		AspectID asp_id() const
		{
			return std::type_index(typeid(*this));
		}

	protected:
		virtual ~Aspect() = 0 { };
		
		// An Object destroys its Aspects by calling asp_dispose() on them.
		// Because of the default implementation given here, this usually just results
		// in the Aspect deleting itself, as expected.
		// However, for subclasses of AspectSelf (see below) this would be disastrous, and needs
		// different behaviour.
		virtual void asp_dispose()
		{
			delete this;
		}

		friend class Object;
	};

	// Let O be a subclass of Object, and A be a subclass of Aspect.
	// If O also derives from AspectSelf<A>, then O can directly implement A's methods.
	// The secret sauce is overriding A's asp_dispose to do absolutely nothing, rather than "delete this".
	// This is because normally, Aspects are separate instances from their containing Objects, and thus need cleanup.
	// However, in this pattern, the Object _is_ the Aspect, and thus we don't want to clean it up twice.
	template<class Base>
	class AspectSelf : public Base
	{
	private:
		// Make private so that an object O can derive from AspectSelf<A1>, and
		// AspectSelf<A2>, and so on and have multiple versions of this method that do
		// not conflict with each other
		virtual void asp_dispose()
		{
		}
	};

	// An Aspect that knows about its containing Object.
	// It's templated for convenience, to allow subclasses of Object to
	// be the container without casting.
	template<class Obj = Object>
	class AspectWithRef : public Aspect
	{
	public:
		AspectWithRef()
			: m_container(nullptr)
		{
		}

		AspectWithRef(Obj* container)
			: m_container(container)
		{
		}

		Obj* asp_container() const
		{
			return m_container;
		}
		
		void asp_set_container(Obj* container)
		{
			m_container = container;
		}

	private:
		Obj* m_container;
	};

	// Same as above, but injects container-referencing ability
	// into an existing Aspect subclass.
	template<class Asp, class Obj = Object>
	class AspectMakeRef : public Asp
	{
	public:
		AspectMakeRef(Obj* container)
			: m_container(container)
		{
		}

		Obj* asp_container() const
		{
			return m_container;
		}

		void asp_set_container(Obj* container)
		{
			m_container = container;
		}

	private:
		Obj* m_container;
	};

	// Base class for all objects that have aspects
	class Object
	{
	public:
		Object()
			: m_aspects(nullptr)
		{
			// m_aspects is allocated lazily only after the first Aspect is added
		}

		~Object()
		{
			if (m_aspects)
			{
				// Clean up each Aspect. Whether it deletes itself is up to it.
				for (auto& i : *m_aspects)
				{
					Aspect* asp = i.second;
					asp->asp_dispose();
				}
			
				delete m_aspects;
			}
		}

		Object(const Object&) = delete; // disable copying, need to call a future clone() function
		
		// Get an Aspect by id
		Aspect* asp_get(AspectID id) const
		{
			if (!m_aspects)
				return asp_not_found_handler(id);

			// Return the aspect pointer. If it's missing, call the
			// not-found handler, the default implementation of which
			// returns nullptr.
			auto where = m_aspects->find(id);
			return where == m_aspects->end()? 
				asp_not_found_handler(id) : 
				where->second;
		}

		// Get an Aspect directly by type (templated version)
		template<class T>
		T* asp_get() const
		{
			return (T*)asp_get(Aspect::asp_id_of<T>());
		}

		// Add an Aspect
		template<class T>
		T* asp_add(T* asp)
		{
			// Lazy initialization, to save memory
			if (!m_aspects)
				m_aspects = new Aspects();

			Aspects& aspects = *m_aspects;

			auto id = Aspect::asp_id_of<T>();
			assert(aspects.count(id) == 0);
			aspects[id] = asp;

			return asp;
		}

		// Add or replace an Aspect
		template<class T>
		T* asp_replace(T* asp)
		{
			if (!m_aspects)
				m_aspects = new Aspects();

			Aspects& aspects = *m_aspects;

			// same as add but without the assert. 
			// replaced aspect gets refcount decreased and possibly gets deleted
			auto asp_base = (Aspect*)asp;
			aspects[Aspect::asp_id_of<T>()] = asp;
			return asp;
		}
		
		// Remove an Aspect
		template<class T>
		void asp_remove()
		{
			if (m_aspects)
			{
				auto it = m_aspects->find(Aspect::asp_id_of<T>());
				if (it != m_aspects->end())
				{
					Aspect* asp = it->second;
					asp->asp_dispose();
					m_aspects->erase(it);

					// Free up memory if this is the last one removed
					// Hopefully this won't cause thrashing of any kind
					if (m_aspects.empty())
					{
						delete m_aspects;
						m_aspects = nullptr;
					}
				}
			}
		}

		// Does this object implement a specific aspect?
		template<class T>
		bool asp_has() const
		{
			return m_aspects && m_aspects->count(Aspect::asp_id_of<T>()) > 0;
		}

		// Return all aspects that are derivable/convertable from type T
		template<class T>
		std::vector<T*> asp_get_all_matching() const
		{
			std::vector<T*> result;

			// Return empty set
			if (!m_aspects)
				return result;

			for (auto asp : *m_aspects)
			{
				T* asp_test = dynamic_cast<T*>(asp.second);
				if (asp_test)
					result.push_back(asp_test);
			}
			return result;
		}

	protected:
		// This is called when aspect_get can't find an aspect. It can be overridden
		// by subclasses to do something clever, like inheritance of apsects.
		virtual Aspect* asp_not_found_handler(const AspectID& id) const
		{
			return nullptr;
		}
		
	private:
		// The map holding this object's Aspects. Done with shared_ptrs to allow ref counting, but
		// this can be changed later transparently.
		// It's allocated dynamically only when needed (once first apsect is added) to save memory.
		typedef std::unordered_map<AspectID, Aspect*> Aspects;
		mutable Aspects* m_aspects;

		// Needed to override how the shared_ptrs stored in the map destroy things.
		static void asp_deleter(Aspect* asp)
		{
			// Ask the aspect to destroy itself. This can be a simiple
			// "delete this" in most cases, but can be overridden.
			// For example, if asp actually points to ths containing object,
			// we definitely want to do nothing, instead of "delete this".
			asp->asp_dispose();
		}
	};
}