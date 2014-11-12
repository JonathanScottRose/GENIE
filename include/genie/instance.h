#pragma once

#include "genie/common.h"

namespace genie
{
	// Attach to an Object that can be instantiated
	class AInstantiable : public Aspect
	{
	public:
		virtual Object* instantiate() = 0;
	};

	// Attach to an instantiated instance, which refers back to its prototype
	class AInstance : public Aspect
	{
	public:
		AInstance(Object* prototype = nullptr)
			: m_prototype(prototype)
		{
		}

		PROP_GET(prototype, Object*, m_prototype);

	protected:
		Object* m_prototype;
	};
}