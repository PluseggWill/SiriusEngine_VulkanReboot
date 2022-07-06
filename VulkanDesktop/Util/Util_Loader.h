#pragma once
#include <fstream>
#include <vector>
#include <string>

namespace UtilLoader
{
	std::vector<char> ReadFile(const std::string& aFilename);
}