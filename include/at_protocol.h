#pragma once

struct ATLinkProtocol
{
	int data_width;
	int addr_width;

	bool has_valid;
	bool has_ready;
	bool is_packet;

	bool compatible_with(const ATLinkProtocol& p) const
	{
		return 
			data_width == p.data_width &&
			addr_width == p.addr_width &&
			has_valid == p.has_valid &&
			has_ready == p.has_ready &&
			is_packet == p.is_packet;
	}
};

