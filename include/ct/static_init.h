#pragma once

#include <functional>
#include <forward_list>

namespace ct
{
	template<class T>
	class StaticInit
	{
	public:
		typedef std::function<void(void)> Func;
		typedef std::pair<Func, Func> Entry;
		typedef std::forward_list<Entry> EntriesType;

	private:
		// There is one set of entries across all instances of this class.
		// It is retrieved with a static function, rather than exist as a static member.
		// This guarantees it will be initialized before it is used.
		static EntriesType& _entries()
		{
			static EntriesType entries;
			return entries;
		}

		static void add(const Func& init, const Func& shutdown)
		{
			_entries().emplace_front(init, shutdown);
		}

	public:
		// Called by host to execute all registered init functions
		static void do_init()
		{
			for (Entry& entry : _entries())
			{
				entry.first();
			}
		}

		// Called by host to execute all registered shutdown functions. Optional.
		static void do_shutdown()
		{
			for (Entry& entry : _entries())
			{
				if (entry.second != nullptr)
				{
					entry.second();
				}
			}
		}

		// Instantiated by client to register init/shutdown behavior.
		StaticInit(const Func& init, const Func& shutdown = Func())
		{
			add(init, shutdown);
		}
	};

/*

// A utility class for registering startup/shutdown code at program startup time, and later
// executing it at a controlled time.

// Instantiate a global/static instance of one of these to register a startup/shutdown function.
// The argument T should be a StaticInitHost.

#define DEFINE_INIT_HOST(name,hname,cname) \
	class name##_init_host_id {}; \
	typedef ct::Util::StaticInitHost<name##_init_host_id> hname; \
	typedef ct::Util::StaticInitClient<hname> cname;

template<typename T>
class StaticInitClient
{
public:
	typedef std::function<void(void)> Func;

	StaticInitClient(const Func& init, const Func& shutdown)
	{
		T::add(init, shutdown);
	}

	StaticInitClient(const Func& init)
	{
		T::add(init, Func());
	}
};

// One of these will receive registrations from all related StaticInitClients. The type T here
// is arbitrary, but should be unique as to differentiate registration lists. To use, simply
// call execute_inits() or execute_shutdowns() when you want all the registered init/shutdown
// calls to be made.

template<typename T>
class StaticInitHost
{
private:
	typedef std::function<void(void)> Func;
	typedef std::pair<Func, Func> Entry;
	typedef std::forward_list<Entry> EntriesType;
	typedef StaticInitHost<T> ThisType;

	// This class needs to be a singleton to ensure that the m_entries list
	// gets initialized before anyone tries to add anything to it.
	EntriesType m_entries;

	static ThisType* inst()
	{
		static ThisType s_inst;
		return &s_inst;
	}

public:
	static void add(const Func& init, const Func& shutdown)
	{
		inst()->m_entries.emplace_front(init, shutdown);
	}

	static void execute_inits()
	{
		for (Entry& entry : inst()->m_entries)
		{
			entry.first();			
		}
	}

	static void execute_shutdowns()
	{
		for (Entry& entry : inst()->m_entries)
		{
			if (entry.second != nullptr)
			{
				entry.second();
			}
		}
	}
};

// A specialized version to execute code at runtime init/shutdown

class InitShutdown
{
public:
	typedef std::function<void(void)> Func;

	InitShutdown(const Func& init, const Func& shutdown = Func())
		: m_shutdown_func(shutdown)
	{
		init();
	}

	~InitShutdown()
	{
		if (m_shutdown_func)
			m_shutdown_func();
	}

protected:
	Func m_shutdown_func;
};

*/

}