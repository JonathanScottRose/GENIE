#include "genie/regex.h"
#include "genie/value.h"

using namespace genie;

Value::Value(const std::vector<int>& vals, int width)
	: m_vals(vals), m_width(width), m_depth(vals.size())
{
}

Value::Value(int val, int width)
	: Value(std::vector<int>{val}, width)
{
}

Value::Value(int val)
	: Value(val, 32)
{
}

Value::Value()
	: Value(0)
{
}

bool Value::operator==(const Value& other) const
{
	return 
		m_vals == other.m_vals &&
		m_width == other.m_width &&
		m_depth == other.m_depth;
}

Value::operator int() const
{
	return get(0);
}

Value& Value::operator= (int val)
{
	set(val);
	return *this;
}

const std::vector<int>& Value::get() const
{
	return m_vals;
}

void Value::set(std::vector<int>& v)
{
	m_vals = v;
	m_depth = v.size();
}

int Value::get(int slice) const
{
	return m_vals[slice];
}

void Value::set(int val, int slice)
{
	m_vals[slice] = val;
}

void Value::set_depth(int depth)
{
	m_depth = depth;
	m_vals.resize(depth);
}

Value::Value(const std::string& val)
{
	int base = 10;

	std::regex regex("\\s*(\\d+)'([dbho])([0-9abcdefABCDEF]+)");
	std::smatch mr;

	std::regex_match(val, mr, regex);
	int bits = std::stoi(mr[1]);
	char radix = mr[2].str().at(0);

	switch (radix)
	{
	case 'd': base = 10; break;
	case 'h': base = 16; break;
	case 'o': base = 8; break;
	case 'b': base = 2; break;
	default: assert(false);
	}
	
	set_width(bits);
	set(std::stoi(mr[3], 0, base));
	m_width = bits;
}

Value Value::parse(const std::string& val)
{
	return Value(val);
}

std::string Value::to_string() const
{
	// We don't support anything else yet
	assert(m_depth == 1);

	// Format for Verilog
	std::string result = std::to_string(m_width);
	result += "'d";
	result += std::to_string(m_vals[0]);

	return result;
}
