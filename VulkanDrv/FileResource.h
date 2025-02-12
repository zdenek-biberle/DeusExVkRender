#pragma once

#include <string>
#include "resource.h"

std::string readShader(unsigned short resource);

class FileResource
{
public:
	static std::string readAllText(const std::string& filename);
};
