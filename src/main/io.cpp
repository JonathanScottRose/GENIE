#include <iostream>
#include <sstream>
#include "io.h"

void io::msg_error(const std::string& what)
{
	std::stringstream ss(what);
	std::string line;

	while (std::getline(ss, line))
	{
		std::cout << "Error: " << line << std::endl;
	}
}

void io::msg_info(const std::string& what)
{
	std::cout << "Info: " << what << std::endl;
}