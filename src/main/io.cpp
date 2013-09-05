#include <iostream>
#include "io.h"

void IO::msg_error(const std::string& what)
{
	std::cout << "Error: " << what << std::endl;
}

void IO::msg_info(const std::string& what)
{
	std::cout << "Info: " << what << std::endl;
}