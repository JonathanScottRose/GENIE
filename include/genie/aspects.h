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

		// Used to make copies of an Aspect, for instantiation of Objects
		virtual Aspect* asp_instantiate() = 0;

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

		// Overridden by Aspects who care when they're attached/moved to a different containing
		// Object
		virtual void asp_set_container(Object*)
		{
		}

		// Object needs to be able to call asp_dispose()
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
		virtual void asp_dispose() override
		{
		}

		Aspect* asp_instantiate() override
		{
			// this is not handled properly yet
			return nullptr;
		}
	};

	// An Aspect that knows about its containing Object.
	// It's templated for convenience, to allow subclasses of Object to
	// be the container without casting.
	template<class OBJ = Object>
	class AspectWithRef : public Aspect
	{
	public:
		AspectWithRef()
			: m_container(nullptr)
		{
		}

		OBJ* asp_container() const
		{
			return m_container;
		}
		
	protected:
		void asp_set_container(Object* container) override
		{
			m_container = static_cast<OBJ*>(container);
			if (!m_container)
				throw Exception("bad attachment");
		}

		friend class Object;

	private:
		OBJ* m_container;
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

		Object(const Object& o)
			: m_aspects(nullptr)
		{
			// Clone all Aspects
			if (o.m_aspects)
			{
				m_aspects = new Aspects();
				for (auto& i : *o.m_aspects)
				{
					asp_add(i.second->asp_instantiate(), i.first);
				}
			}
		}
		
		// Get an Aspect by id
		Aspect* asp_get(AspectID id) const
		{
			if (!m_aspects)
				return asp_not_found_handler(id);

			// Return the aspect pointer. If it's missing, call the
			// not-found handler, the default implementation of which
			// returns nullptr.
			auto where = asp_find(id);
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

		// Add an Aspect to this Object
		template<class T>
		T* asp_add(T* asp)
		{
			// Use templated type T, instead of asp object's run-time type, as the key
			// for insertion. Allowing these to differ gives fun possibilities!
			auto id = Aspect::asp_id_of<T>();
			
			return static_cast<T*>(asp_add(asp, id));
		}

		Aspect* asp_add(Aspect* asp, AspectID id)
		{
			// Lazy initialization, to save memory
			if (!m_aspects)
				m_aspects = new Aspects();

			// Ensure no existing entry
			assert(asp_find(id) == m_aspects->end());

			// Insert
			m_aspects->emplace_back(id, asp);

			// Inform Aspect of its new container (if it cares)
			asp->asp_set_container(this);

			return asp;
		}
		
		// Remove an Aspect
		template<class T>
		T* asp_remove()
		{
			T* result = nullptr;

			if (m_aspects)
			{
				auto it = asp_find(Aspect::asp_id_of<T>());
				if (it != m_aspects->end())
				{
					Aspect* result = it->second;
					result->asp_dispose();
					m_aspects->erase(it);

					// Free up memory if this is the last one removed
					// Hopefully this won't cause thrashing of any kind
					if (m_aspects->empty())
					{
						delete m_aspects;
						m_aspects = nullptr;
					}
				}
			}

			return result;
		}

		// Does this object implement a specific aspect?
		template<class T>
		bool asp_has() const
		{
			return m_aspects && asp_find(Aspect::asp_id_of<T>()) != m_aspects->end();
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
		// by subclasses to do something clever, like inheritance of apsects from other objects.
		virtual Aspect* asp_not_found_handler(const AspectID& id) const
		{
			return nullptr;
		}
		
	private:
		// The map holding this object's Aspects. 
		// It's allocated dynamically only when needed (once first apsect is added) to save memory.
		typedef std::vector<std::pair<AspectID, Aspect*>> Aspects;
		mutable Aspects* m_aspects;

		// Utility function
		Aspects::iterator asp_find(AspectID id) const
		{
			return std::find_if(m_aspects->begin(), m_aspects->end(), 
				[=](const Aspects::value_type& e)
			{
				return e.first == id;
			});
		}
	};
}