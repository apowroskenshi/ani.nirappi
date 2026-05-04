#pragma once

#include <string>

#include "renderer/Shader.h"

class BlurShader : public Shader {
public:
	BlurShader(const std::string& computeShaderFile, bool horizontal);
};
