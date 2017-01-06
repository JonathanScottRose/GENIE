#pragma once

#include <vector>

namespace genie
{
namespace impl
{
    class BitsVal
    {
    public:
        enum Base
        {
            BIN,
            DEC,
            HEX
        };

        BitsVal();
        BitsVal(const BitsVal&) = default;
        BitsVal(BitsVal&&) = default;
        BitsVal(unsigned inner_dim, unsigned outer_dim = 1);
        ~BitsVal();

        BitsVal& operator=(const BitsVal&) = default;
        BitsVal& operator=(BitsVal&&) = default;

        void set_preferred_base(Base b);
        Base get_preferred_base() const;

        void set_size(unsigned size, unsigned dim=0);
        unsigned get_size(unsigned dim=0) const;

        unsigned get_bit(unsigned pos, unsigned slice=0) const;
        void set_bit(unsigned pos, unsigned bit);
        void set_bit(unsigned pos, unsigned slice, unsigned bit);
        void shift_in_lsb(unsigned bit, unsigned slice=0);

    protected:
        void set_slice_size(unsigned slice, unsigned size);

        Base m_base;
        unsigned m_n_dims;
        unsigned m_dim_size[2];

        std::vector<std::vector<size_t>> m_data;        
    };
}
}