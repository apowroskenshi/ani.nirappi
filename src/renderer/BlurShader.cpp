#include "renderer/BlurShader.h"
#include "core/Logger.h"

BlurShader::BlurShader(const std::string& computeShaderFile, bool horizontal)
{
	std::string computeCode = readShaderFile(computeShaderFile);

	std::string header = "#version 430 core\n";
	header += horizontal ? "#define HORIZONTAL\n" : "#define VERTICAL\n";

	const char* sources[2] = { header.c_str(), computeCode.c_str() };

	GLuint compute = glCreateShader(GL_COMPUTE_SHADER);
	glShaderSource(compute, 2, sources, nullptr);
	glCompileShader(compute);
	checkCompileErrors(compute, "COMPUTE");

	m_id = glCreateProgram();
	glAttachShader(m_id, compute);
	glLinkProgram(m_id);
	checkCompileErrors(m_id, "PROGRAM");

	glDeleteShader(compute);

	LOG_DEBUG << "Blur compute shader compiled successfully";
}
