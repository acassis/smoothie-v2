#pragma once

#include <vector>
#include <tuple>

#include "GCode.h"

class GCodeProcessor
{
public:
	GCodeProcessor();
	~GCodeProcessor();

	using GCodes_t = std::vector<GCode>;

	bool parse(const char *line, GCodes_t& gcodes);
	int get_line_number() const { return line_no; }

    static std::tuple<uint16_t, uint16_t, float> parse_code(const char *&p);

private:
	// modal settings
	GCode group1;
	int line_no;
};
