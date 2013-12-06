#include <iostream>
#include "ct/common.h"
#include "io.h"
#include "getoptpp/getopt_pp.h"
#include "globals.h"

using namespace ct;


namespace
{
	std::string s_script;

	void parse_args(int argc, char** argv)
	{
		GetOpt::GetOpt_pp args(argc, argv);

		//args >> GetOpt::Option('p', "component_path", Globals::inst()->component_path);
		
		if (!(args >> GetOpt::GlobalOption(s_script)))
			throw Exception("Must specify script");
	}

}


int main(int argc, char** argv)
{
	try
	{
		parse_args(argc, argv);
	}
	catch (std::exception& e)
	{
		IO::msg_error(e.what());		
	}

	return 0;
}
