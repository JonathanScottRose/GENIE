#pragma once

#include "genie/common.h"

namespace genie
{
	class Value
	{
	public:
		Value(const std::string&);
		Value(const std::vector<int>&, int width);
		Value(int val, int width);
		Value(int val);
		Value();
		~Value() = default;

		static Value parse(const std::string&);

		Value& operator=(int v);
		bool operator==(const Value&) const;
		operator int() const;

		const std::vector<int>& get() const;
		void set(std::vector<int>&);

		int get(int slice = 0) const;
		void set(int val, int slice=0);

		PROP_GET_SET(width, int, m_width);
		PROP_GET(depth, int, m_depth);
		void set_depth(int);

	protected:
		std::vector<int> m_vals;
		int m_width;
		int m_depth;
	};
}