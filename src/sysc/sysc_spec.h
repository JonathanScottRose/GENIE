#pragma once

#include <systemc.h>
#include "common.h"
#include "ct/macros.h"

using namespace ct;

namespace ct
{
namespace SysC
{
	
struct CompInfo : public OpaqueDeletable
{
	InsterFunc inster;
};

struct LinkpointInfo : public OpaqueDeletable
{
	int encoding;
};

template<class T>
struct SignalInfo : public OpaqueDeletable
{
	T binder;
};

typedef SignalInfo<ClockBinder> ClockSignalInfo;
typedef SignalInfo<VecBinder> VecSignalInfo;
typedef SignalInfo<BoolBinder> BoolSignalInfo;

}
}