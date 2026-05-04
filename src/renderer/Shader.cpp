#include "renderer/Shader.h"

#include <fstream>
#include <sstream>

Shader::Shader(const std::string& vertexShaderFile, const std::string& fragmentShaderFile)
{
	std::string vertexCode = readShaderFile(vertexShaderFile);
	std::string fragmentCode = readShaderFile(fragmentShaderFile);

	GLuint vertex = compileShader(GL_VERTEX_SHADER, vertexCode.c_str());
	GLuint fragment = compileShader(GL_FRAGMENT_SHADER, fragmentCode.c_str());

	m_id = glCreateProgram();
	glAttachShader(m_id, vertex);
	glAttachShader(m_id, fragment);
	glLinkProgram(m_id);
	checkCompileErrors(m_id, "PROGRAM");

	glDeleteShader(vertex);
	glDeleteShader(fragment);

	LOG_DEBUG << "Shaders compiled successfully";
}

Shader::Shader(const std::string& computeShaderFile)
{
	std::string computeCode = readShaderFile(computeShaderFile);
	GLuint compute = compileShader(GL_COMPUTE_SHADER, computeCode.c_str());

	m_id = glCreateProgram();
	glAttachShader(m_id, compute);
	glLinkProgram(m_id);
	checkCompileErrors(m_id, "PROGRAM");

	glDeleteShader(compute);

	LOG_DEBUG << "Compute shader compiled successfully";
}

Shader::~Shader()
{
	if (m_id != 0) {
		glDeleteProgram(m_id);
	}
}

Shader::Shader(Shader&& other) noexcept
	: m_id(other.m_id)
	, m_uniformCache(std::move(other.m_uniformCache))
{
	other.m_id = 0;
}

Shader& Shader::operator=(Shader&& other) noexcept
{
	if (this != &other) {
		if (m_id != 0) {
			glDeleteProgram(m_id);
		}
		m_id = other.m_id;
		m_uniformCache = std::move(other.m_uniformCache);
		other.m_id = 0;
	}
	return *this;
}

void Shader::use() const
{
	glUseProgram(m_id);
}

// --- Uniform setters ---

void Shader::setBool(const std::string& name, bool value) const
{
	glUniform1i(getUniformLocation(name), static_cast<int>(value));
}

void Shader::setInt(const std::string& name, int value) const
{
	glUniform1i(getUniformLocation(name), value);
}

void Shader::setFloat(const std::string& name, float value) const
{
	glUniform1f(getUniformLocation(name), value);
}

void Shader::setFloatArray(const std::string& name, int count, const float* data) const
{
	glUniform1fv(getUniformLocation(name), count, data);
}

void Shader::setVec2(const std::string& name, const glm::vec2& value) const
{
	glUniform2fv(getUniformLocation(name), 1, &value[0]);
}

void Shader::setVec2(const std::string& name, float x, float y) const
{
	glUniform2f(getUniformLocation(name), x, y);
}

void Shader::setVec3(const std::string& name, const glm::vec3& value) const
{
	glUniform3fv(getUniformLocation(name), 1, &value[0]);
}

void Shader::setVec3(const std::string& name, float x, float y, float z) const
{
	glUniform3f(getUniformLocation(name), x, y, z);
}

void Shader::setVec4(const std::string& name, const glm::vec4& value) const
{
	glUniform4fv(getUniformLocation(name), 1, &value[0]);
}

void Shader::setVec4(const std::string& name, float x, float y, float z, float w) const
{
	glUniform4f(getUniformLocation(name), x, y, z, w);
}

void Shader::setMat2(const std::string& name, const glm::mat2& mat) const
{
	glUniformMatrix2fv(getUniformLocation(name), 1, GL_FALSE, &mat[0][0]);
}

void Shader::setMat3(const std::string& name, const glm::mat3& mat) const
{
	glUniformMatrix3fv(getUniformLocation(name), 1, GL_FALSE, &mat[0][0]);
}

void Shader::setMat4(const std::string& name, const glm::mat4& mat) const
{
	glUniformMatrix4fv(getUniformLocation(name), 1, GL_FALSE, &mat[0][0]);
}

// --- Private helpers ---

GLint Shader::getUniformLocation(const std::string& name) const
{
	auto it = m_uniformCache.find(name);
	if (it != m_uniformCache.end()) {
		return it->second;
	}

	GLint location = glGetUniformLocation(m_id, name.c_str());
	m_uniformCache[name] = location;
	return location;
}

GLuint Shader::compileShader(GLenum type, const char* source) const
{
	GLuint shader = glCreateShader(type);
	glShaderSource(shader, 1, &source, nullptr);
	glCompileShader(shader);

	const char* typeName = "UNKNOWN";
	switch (type) {
		case GL_VERTEX_SHADER:   typeName = "VERTEX";   break;
		case GL_FRAGMENT_SHADER: typeName = "FRAGMENT"; break;
		case GL_COMPUTE_SHADER:  typeName = "COMPUTE";  break;
	}
	checkCompileErrors(shader, typeName);

	return shader;
}

// --- Protected helpers ---

std::string Shader::readShaderFile(const std::string& fileName) const
{
	std::filesystem::path filePath = kShaderDir / fileName;
	std::ifstream file(filePath);

	if (!file.is_open()) {
		LOG_ERROR << "Failed to open shader file: " << filePath.string();
		return "";
	}

	std::stringstream stream;
	stream << file.rdbuf();
	return stream.str();
}

std::string Shader::getShaderFilePath(const std::string& shaderFileName) const
{
	return (kShaderDir / shaderFileName).string();
}

void Shader::checkCompileErrors(GLuint shader, const std::string& type) const
{
	GLint success;
	GLchar infoLog[1024];

	if (type != "PROGRAM") {
		glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
		if (!success) {
			glGetShaderInfoLog(shader, sizeof(infoLog), nullptr, infoLog);
			LOG_ERROR << "Shader compilation error (" << type << "): " << infoLog;
		}
	}
	else {
		glGetProgramiv(shader, GL_LINK_STATUS, &success);
		if (!success) {
			glGetProgramInfoLog(shader, sizeof(infoLog), nullptr, infoLog);
			LOG_ERROR << "Program linking error: " << infoLog;
		}
	}
}
