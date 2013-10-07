#pragma once

#include <systemc.h>
#include "common.h"
#include "ct/macros.h"

using namespace ct;

namespace ct
{
namespace SysC
{
	
struct CompInfo : public ImplAspect
{
	InsterFunc inster;
};

struct LinkpointInfo : public ImplAspect
{
	int encoding;
};

template<class T>
struct SignalInfo : public ImplAspect
{
	T binder;
};

typedef SignalInfo<ClockBinder> ClockSignalInfo;
typedef SignalInfo<VecBinder> VecSignalInfo;
typedef SignalInfo<BoolBinder> BoolSignalInfo;

}
}