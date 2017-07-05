#include "pch.h"
#include "prim_db.h"

using namespace genie::impl;

static constexpr unsigned TNODE_GUARD_VAL = std::numeric_limits<unsigned>::max();

PrimDB::PrimDB()
	: m_tnode_data(nullptr)
{
}

PrimDB::~PrimDB()
{
	delete[] m_tnode_data;
	m_tnode_data = nullptr;
}

void PrimDB::initialize(const std::string& filename,
	const SmartEnumTable& col_enum, const SmartEnumTable& tnode_enum_src,
	const SmartEnumTable& tnode_enum_sink)
{
	FILE* fp = fopen(filename.c_str(), "r");
	if (!fp)
	{
		throw Exception("Couldn't open database " + filename);
	}

	// State
	unsigned n_cols;
	unsigned n_rows;
	std::vector<unsigned> col_map(col_enum.size());

	// Get and validate number of columns
	auto fret = fscanf(fp, " COLS %u", &n_cols);
	assert(fret == 1);
	assert(n_cols == col_enum.size());
	m_n_cols = n_cols;

	// Create mapping between file column names and enum column name order
	for (unsigned i = 0; i < n_cols; i++)
	{
		char col_name[128];
		fret = fscanf(fp, " %128[^ \n]", col_name);
		assert(fret == 1);

		// Lookup enum for string
		unsigned enum_pos = 0;
		bool bret = col_enum.from_string(col_name, enum_pos);
		assert(bret);

		// Column i in file is this column enum
		col_map[i] = enum_pos;
	}

	// Get number of rows
	fret = fscanf(fp, " ROWS %u", &n_rows);
	assert(fret == 1);

	// Calculate size of tnode block for each row and allocate
	// tnode data
	m_n_tnode_src = tnode_enum_src.size();
	m_n_tnode_sink = tnode_enum_sink.size();
	
	unsigned tnode_blk_size = m_n_tnode_src * m_n_tnode_sink;
	m_tnode_data = new unsigned[n_rows * tnode_blk_size];
	//memset(m_tnode_data, TNODE_GUARD_VAL, tnode_blk_size * n_rows * sizeof(unsigned));
	memset(m_tnode_data, 0, tnode_blk_size * n_rows * sizeof(unsigned));
	
	// Get row data
	for (unsigned rowno = 0; rowno < n_rows; rowno++)
	{
		// Get column vals
		std::vector<unsigned> col_vals(n_cols);
		for (unsigned colno = 0; colno < n_cols; colno++)
		{
			unsigned col_val;
			fret = fscanf(fp, " %u", &col_val);
			assert(fret == 1);
			col_vals[col_map[colno]] = col_val;
		}

		// Get row hash for column vals
		RowHash rowhash = hash_row(col_vals.data(), n_cols);

		// Create a rowdata in the hash table
		auto ins_result = m_row_data.emplace(std::make_pair(rowhash, RowData()));
		auto row_it = ins_result.first;
		if (!ins_result.second)
			throw Exception("row hash collision!");

		RowData& rowdata = row_it->second;
		
		// Parse resources TODO make more flexible
		fret = fscanf(fp, " ALM %u MemALM %u CombALUT %u Reg %u",
			&rowdata.alm, &rowdata.mem_alm, &rowdata.comb, &rowdata.reg);
		assert(fret == 4);

		// Set tnodes pointer. Each row has the same number of tnode pairs for now in memory.
		rowdata.tnode_ofs = rowno * tnode_blk_size;

		// Get number of actual tnodes in file
		unsigned n_file_tnodes;
		fret = fscanf(fp, " NODES %u", &n_file_tnodes);
		assert(fret == 1);

		// Parse the nodes
		for (unsigned nodeno = 0; nodeno < n_file_tnodes; nodeno++)
		{
			char src_str[128];
			char sink_str[128];
			unsigned tnode_val;
			fret = fscanf(fp, " %128[^ \n] %128[^ \n] %u", src_str, sink_str, &tnode_val);
			assert(fret == 3);

			// Get enum-based indices
			unsigned src_enum_pos;
			unsigned sink_enum_pos;
			bool bret = tnode_enum_src.from_string(src_str, src_enum_pos); assert(bret);
			bret = tnode_enum_sink.from_string(sink_str, sink_enum_pos); assert(bret);

			// Put in table
			unsigned tab_pos = src_enum_pos*m_n_tnode_sink + sink_enum_pos;
			m_tnode_data[rowdata.tnode_ofs + tab_pos] = tnode_val;
		}
	}

	fclose(fp);
}

auto PrimDB::hash_row(ColumnVal cols[], unsigned ncols) -> RowHash
{
	// Hash the columns
	size_t hash = ncols; // initialize with vec size
	for (unsigned i = 0; i < ncols; i++)
	{
		hash ^= (size_t)cols[i] + 0x9e3779b9UL + (hash << 6) + (hash >> 2);
	}
	return hash;
}

auto PrimDB::get_row(ColumnVal cols[]) -> RowHandle
{
	RowHash hash = hash_row(cols, m_n_cols);

	// Lookup in row data hash table (this hashes it again... oh well)
	auto it = m_row_data.find(hash);
	return (it == m_row_data.end()) ? nullptr :
		static_cast<RowData*>(&it->second);
}

AreaMetrics* PrimDB::get_area_metrics(RowHandle row)
{
	assert(row != ROW_INVALID);
	// Cast to derived private type instead of directly to areametrics base type
	return static_cast<RowData*>(row);
}

auto PrimDB::get_tnodes(RowHandle row) -> TNodesHandle
{
	assert(row != ROW_INVALID);
	auto rowdata = static_cast<RowData*>(row);

	// Tnode handle is just a pointer into the tnodes table, dictated by offset
	return m_tnode_data + rowdata->tnode_ofs;
}

unsigned PrimDB::get_tnode_val(TNodesHandle tnodes, unsigned src, unsigned sink)
{
	assert(src < m_n_tnode_src);
	assert(sink < m_n_tnode_sink);

	unsigned idx = src*m_n_tnode_sink + sink;
	unsigned* base = tnodes;
	unsigned result = base[idx];
	// Check for existence
	//assert(result != TNODE_GUARD_VAL);
	return result;
}

AreaMetrics AreaMetrics::operator+(const AreaMetrics &o) const
{
	AreaMetrics result = *this;
	result += o;
	return result;
}

AreaMetrics AreaMetrics::operator-(const AreaMetrics &o) const
{
	AreaMetrics result = *this;
	result -= o;
	return result;
}

AreaMetrics genie::impl::AreaMetrics::operator*(unsigned x) const
{
	AreaMetrics result = *this;
	result *= x;
	return result;
}

AreaMetrics & AreaMetrics::operator+=(const AreaMetrics &o)
{
	this->alm += o.alm;
	this->comb += o.comb;
	this->reg += o.reg;
	this->mem_alm += o.mem_alm;
	return *this;
}

AreaMetrics & AreaMetrics::operator-=(const AreaMetrics &o)
{
	//assert(this->alm >= o.alm);
	assert(this->comb >= o.comb);
	assert(this->reg >= o.reg);
	assert(this->mem_alm >= o.mem_alm);
	//this->alm -= o.alm;
	this->comb -= o.comb;
	this->reg -= o.reg;
	this->mem_alm -= o.mem_alm;
	return *this;
}

AreaMetrics & genie::impl::AreaMetrics::operator*=(unsigned x)
{
	this->alm *= x;
	this->comb *= x;
	this->reg *= x;
	this->mem_alm *= x;
	return *this;
}

