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

		//TODO: make port's dimension a single type and arg
		// allows having a constructor give an initial value
        BitsVal();
        BitsVal(const BitsVal&) = default;
        BitsVal(BitsVal&&) = default;
        BitsVal(unsigned inner_dim, unsigned outer_dim = 1);
        ~BitsVal();

        BitsVal& operator=(const BitsVal&);
        BitsVal& operator=(BitsVal&&);

        void set_preferred_base(Base b);
        Base get_preferred_base() const;

        void set_size(unsigned size, unsigned dim=0);
        unsigned get_size(unsigned dim=0) const;

        unsigned get_bit(unsigned pos, unsigned slice=0) const;
        BitsVal& set_bit(unsigned pos, unsigned bit);
		BitsVal& set_bit(unsigned pos, unsigned slice, unsigned bit);
		BitsVal& set_val(unsigned pos, unsigned slice, unsigned val, unsigned bits);
		BitsVal& shift_in_lsb(unsigned bit, unsigned slice=0);

        std::string to_str_bin(unsigned slice) const;
        std::string to_str_dec(unsigned slice) const;
        std::string to_str_hex(unsigned slice) const;

    protected:
        static constexpr unsigned CHUNK_SIZE = sizeof(size_t)*8;
        unsigned get_n_chunks(unsigned bits) const;
        void set_slice_size(unsigned slice, unsigned size);

        Base m_base;
        unsigned m_n_dims;
        unsigned m_dim_size[2];

        std::vector<std::vector<size_t>> m_data;        
    };
}
}