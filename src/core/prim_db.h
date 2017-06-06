#pragma once

#include <unordered_map>
#include "genie/smart_enum.h"

namespace genie
{
namespace impl
{
	struct AreaMetrics
	{
		unsigned alm;
		unsigned reg;
		unsigned comb;
		unsigned mem_alm;
	};

	class PrimDB
	{
	public:
		using ColumnVal = unsigned;
		using RowHandle = void*;
		using TNodesHandle = unsigned*;

		static constexpr RowHandle ROW_INVALID = nullptr;

		PrimDB();
		~PrimDB();

		void initialize(const std::string& filename,
			const SmartEnumTable& col_enum, const SmartEnumTable& tnode_enum_src,
			const SmartEnumTable& tnode_enum_sink);

		RowHandle get_row(ColumnVal[]);
		AreaMetrics* get_area_metrics(RowHandle);
		TNodesHandle get_tnodes(RowHandle);
		unsigned get_tnode_val(TNodesHandle, unsigned src, unsigned dest);

	protected:
		using RowHash = size_t;

		struct RowData : public AreaMetrics
		{
			unsigned tnode_ofs;
		};

		RowHash hash_row(ColumnVal cols[], unsigned ncols);

		unsigned* m_tnode_data;
		std::unordered_map<RowHash, RowData> m_row_data;
		unsigned m_n_cols;
		unsigned m_n_tnode_src;
		unsigned m_n_tnode_sink;
	};
}
}