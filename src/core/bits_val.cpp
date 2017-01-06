#include "pch.h"
#include "genie/genie.h"
#include "bits_val.h"

using namespace genie::impl;

BitsVal::BitsVal()
    : BitsVal(0, 1)
{
}

BitsVal::BitsVal(unsigned inner_dim, unsigned outer_dim)
    : m_base(BIN), m_dim_size{inner_dim, outer_dim}
{
    m_n_dims = outer_dim==1? 1 : 2;
    set_size(outer_dim, 1);
    set_size(inner_dim, 0);
}

BitsVal::~BitsVal()
{
}

void BitsVal::set_preferred_base(Base b)
{
    m_base = b;
}

BitsVal::Base BitsVal::get_preferred_base() const
{
    return m_base;
}

void BitsVal::set_slice_size(unsigned slice, unsigned size)
{
    unsigned BLKSIZE = sizeof(size_t)*8;
    unsigned size_in_blks = (size + BLKSIZE - 1) / BLKSIZE;
    
    // Coarse resize
    m_data[slice].resize(size_in_blks);

    // Chop of trailing bits
    unsigned ofs = size % BLKSIZE;
    size_t mask = (1 << size) - 1;
    m_data[slice].back() &= mask;
}

void BitsVal::set_size(unsigned size, unsigned dim)
{
    if (dim == 0)
    {
        for (unsigned i = 0; i < m_dim_size[1]; i++)
        {
            set_slice_size(i, size);
        }
    }
    else if (dim == 1)
    {
        unsigned i = m_data.size() - 1;
        m_data.resize(size);

        for (; i < m_data.size(); i++)
        {
            set_slice_size(i, size);
        }
    }
    else
    {
        throw Exception("dimension > 1");
    }

    m_dim_size[dim] = size;        
}

unsigned BitsVal::get_size(unsigned dim) const
{
    assert(dim == 0 || dim == 1);
    return m_dim_size[dim];
}

unsigned BitsVal::get_bit(unsigned pos, unsigned slice) const
{
    size_t chunk = m_data[slice][pos];
    unsigned ofs = pos % (sizeof(size_t)*8);
    return (chunk >> ofs) & 1;
}

void BitsVal::set_bit(unsigned pos, unsigned bit)
{
    set_bit(pos, 0, bit);
}

void BitsVal::set_bit(unsigned pos, unsigned slice, unsigned bit)
{
    size_t chunk = m_data[slice][pos];
    unsigned ofs = pos % (sizeof(size_t)*8);
    chunk &= (size_t)(-1) ^ (1 << ofs);
    chunk |= (bit & 1) << ofs;
}

void BitsVal::shift_in_lsb(unsigned bit, unsigned slice)
{
    bit &= 1;

    for (unsigned i = 0; i < m_dim_size[0]; i++)
    {
        unsigned shift_out = m_data[slice][i] >> (sizeof(size_t)*8 - 1);
        m_data[slice][i] = (m_data[slice][i] << 1) | bit;
        bit = shift_out;
    }
}


