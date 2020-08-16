#include "IO.h"

#include <fstream>

#include <Singularity.Core/CoreDeclare.h>

namespace Singularity
{
	namespace IO
	{
		std::vector<char> ReadFile(const std::string& _filename)
		{
			std::ifstream fileStream(_filename, std::ios::ate | std::ios::binary);

			if (!fileStream.is_open()) {
				throw std::runtime_error("failed to open file!");
			}

			size_t fileSize = (size_t)fileStream.tellg();
			std::vector<char> buffer(fileSize);

			fileStream.seekg(0);
			fileStream.read(buffer.data(), fileSize);

			fileStream.close();

			return buffer;
		}
	}
}
