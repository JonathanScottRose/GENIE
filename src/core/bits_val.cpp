#include "pch.h"
#include "genie/genie.h"
#include "bits_val.h"

using namespace genie::impl;

BitsVal::BitsVal()
    : BitsVal(1, 1)
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

BitsVal & BitsVal::operator=(const BitsVal &rhs)
{
    m_base = rhs.m_base;
    m_n_dims = rhs.m_n_dims;
    m_dim_size[0] = rhs.m_dim_size[0];
    m_dim_size[1] = rhs.m_dim_size[1];
    m_data = rhs.m_data;
    return *this;
}

BitsVal & BitsVal::operator=(BitsVal &&rhs)
{
    m_base = rhs.m_base;
    m_n_dims = rhs.m_n_dims;
    m_dim_size[0] = rhs.m_dim_size[0];
    m_dim_size[1] = rhs.m_dim_size[1];
    m_data = std::move(rhs.m_data);

    return *this;
}

void BitsVal::set_preferred_base(Base b)
{
    m_base = b;
}

BitsVal::Base BitsVal::get_preferred_base() const
{
    return m_base;
}

unsigned genie::impl::BitsVal::get_n_chunks(unsigned bits) const
{
    return (bits + CHUNK_SIZE - 1) / CHUNK_SIZE;
}

void BitsVal::set_slice_size(unsigned slice, unsigned size)
{
    unsigned size_in_blks = get_n_chunks(size);
    
    // Coarse resize
    m_data[slice].resize(size_in_blks);

    // Chop of trailing bits
    unsigned ofs = size % CHUNK_SIZE;
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
        unsigned i = m_data.size();
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
    size_t chunk = m_data[slice][pos / CHUNK_SIZE];
    unsigned ofs = pos % CHUNK_SIZE;
    return (chunk >> ofs) & 1;
}

BitsVal& BitsVal::set_bit(unsigned pos, unsigned bit)
{
    set_bit(pos, 0, bit);
	return *this;
}

BitsVal& BitsVal::set_bit(unsigned pos, unsigned slice, unsigned bit)
{
    size_t& chunk = m_data[slice][pos / CHUNK_SIZE];
    unsigned ofs = pos % CHUNK_SIZE;
    chunk &= (size_t)(-1) ^ (1 << ofs);
    chunk |= (bit & 1) << ofs;
	return *this;
}

BitsVal& BitsVal::set_val(unsigned pos, unsigned slice, unsigned val, unsigned bits)
{
	assert(bits <= CHUNK_SIZE);
	// Can't straddle chunk boundaries yet
	assert( (pos % CHUNK_SIZE) == 0);

	auto& chunk = m_data[slice][pos / CHUNK_SIZE];
	chunk &= ~((1<<bits)-1);
	chunk |= val;
	return *this;
}

BitsVal& BitsVal::shift_in_lsb(unsigned bit, unsigned slice)
{
    bit &= 1;

    for (unsigned i = 0; i < m_dim_size[0]; i++)
    {
        unsigned shift_out = m_data[slice][i] >> (CHUNK_SIZE - 1);
        m_data[slice][i] = (m_data[slice][i] << 1) | bit;
        bit = shift_out;
    }
	return *this;
}

std::string BitsVal::to_str_bin(unsigned slice) const
{
    std::string result;
    for (int b = m_dim_size[0]-1; b >= 0; b--)
    {
        result += std::to_string(get_bit(b, slice));
    }
    return result;
}

std::string BitsVal::to_str_dec(unsigned slice) const
{
    // This won't work for anything larger than 64 bits
    unsigned long long result = 0;
    unsigned chunks = get_n_chunks(m_dim_size[0]);
    assert(chunks * CHUNK_SIZE <= 64);
    for (unsigned i = 0; i < chunks; i++)
    {
        result += m_data[slice][i] << (CHUNK_SIZE*i);
    }
    
    return std::to_string(result);
}

std::string BitsVal::to_str_hex(unsigned slice) const
{
    std::stringstream ss;
    
    unsigned chunks = get_n_chunks(m_dim_size[0]);
    for (int i = chunks-1; i >= 0; i--)
    {
        size_t chunkval = m_data[slice][i];
        ss << std::hex << chunkval;
    }

    return ss.str();
}


