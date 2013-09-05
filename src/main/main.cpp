#include <stdexcept>
#include <iostream>
#include "getoptpp/getopt_pp.h"
#include "globals.h"
#include "build_spec.h"
#include "io.h"
#include "ct/ct.h"
#include "write_verilog.h"

namespace
{
	void parse_args(int argc, char** argv)
	{
		GetOpt::GetOpt_pp args(argc, argv);

		args >> GetOpt::Option('p', "component_path", Globals::inst()->component_path);
		
		if (!(args >> GetOpt::GlobalOption(Globals::inst()->input_files)))
			throw std::exception("Must specify input .xml files");
	}

}



int main(int argc, char** argv)
{
	try
	{
		parse_args(argc, argv);
		BuildSpec::go();
		ct::go();
		WriteVerilog::go();
	}
	catch (std::exception& e)
	{
		IO::msg_error(e.what());		
	}

	return 0;
}
