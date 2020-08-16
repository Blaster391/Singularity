#pragma once
#include <vector>
#include <string>

namespace Singularity
{
	namespace IO
	{
		std::vector<char> ReadFile(const std::string& _filename);
	}
}
