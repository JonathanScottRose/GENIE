#pragma once

#include "ct/spec.h"

using namespace ct;

namespace BuildSpec
{
	struct ComponentImpl : public ImplAspect
	{
		std::string module_name;
	};

	struct SignalImpl : public ImplAspect
	{
		std::string signal_name;
	};

	struct LinkpointImpl : public ImplAspect
	{
		std::string encoding;
	};
}