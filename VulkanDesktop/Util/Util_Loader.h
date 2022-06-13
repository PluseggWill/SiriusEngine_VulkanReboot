#pragma once
#include <fstream>
#include <vector>
#include <string>

class Util_Loader
{
public:
	static std::vector<char> readFile(const std::string& aFilename);
};