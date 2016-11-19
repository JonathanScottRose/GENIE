#pragma once

#include <functional>
#include <forward_list>

namespace genie
{
	// Basic Static Initializer.
	// Holds a list of things of type Entry that can be added to by instantiating an instance
	// of this class.
	// Later, all entries can be retrieved with a static call to entries()
	//
	// The type Token is used to create a unique class (and therefore a unique list of entries), if
	// there is a need to disambiguate betwen multiple StaticInitBase<Entry> classes
	//
	template<class Entry, class Token=Entry>
	class StaticInitBase
	{
	public:
		typedef std::forward_list<Entry> EntriesType;

		static EntriesType& entries()
		{
			static EntriesType entries;
			return entries;
		}

		// Instantiated statically to add entry to the list
		StaticInitBase(const Entry& entry)
		{
			entries().emplace_front(entry);
		}

		StaticInitBase() = delete;
		StaticInitBase(const StaticInitBase&) = delete;
		~StaticInitBase() = default;
	};

	// Registrar idiom.
	//
	// These two classes work together to allow code to register instances of a particular
	// superclass, and have the instances actually be created at some later, deferred time.
	//
	// For a base type BaseT, the class StaticRegistry<BaseT> holds all such entries. They are
	// functions that can be called (by looking at the ::entries() static member), each of which
	// calls a constructor to create an instance of some subclass of BaseT.
	//
	// By creating an instance of StaticRegistryEntry<T, BaseT>, adds an entry for T in
	// StaticRegistry<BaseT>'s list.
	//	
	template<class BaseT, class Token=BaseT>
	using StaticRegistry = StaticInitBase<std::function<BaseT*(void)>, Token>;

	template<class T, class BaseT, class Token=BaseT>
	class StaticRegistryEntry
	{
	public:
		typedef StaticRegistry<BaseT, Token> Registry;

		StaticRegistryEntry()
		{
			// Register a function that creates and returns a new instance of T
			Registry::entries().push_front([]()
			{
				return new T();
			});
		}

		StaticRegistryEntry(const StaticRegistryEntry&) = delete;
		~StaticRegistryEntry() = default;
	};
}