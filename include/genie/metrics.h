#pragma once

namespace genie
{
    struct AreaMetrics
    {
        unsigned luts = 0;
        unsigned regs = 0;
        unsigned dist_ram = 0;

        AreaMetrics operator+(const AreaMetrics& o) const
        {
            AreaMetrics result = *this;
            result.luts += o.luts;
            result.regs += o.regs;
            result.dist_ram += o.dist_ram;
            return result;
        }

        AreaMetrics& operator +=(const AreaMetrics& o)
        {
            *this = *this + o;
            return *this;
        }
    };
}