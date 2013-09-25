#pragma once

#include "ct/spec.h"

using namespace ct;

namespace BuildSpec
{
	struct ComponentImpl : public OpaqueDeletable
	{
		std::string module_name;
	};

	struct SignalImpl : public OpaqueDeletable
	{
		std::string signal_name;
	};

	struct LinkpointImpl : public OpaqueDeletable
	{
		std::string encoding;
	};

	void go();
}