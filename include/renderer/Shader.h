#pragma once

#include <string>
#include <filesystem>
#include <unordered_map>

#include <glm/glm.hpp>
#include <glad/glad.h>

#include "core/Logger.h"

class Shader {
public:
	Shader() = default;
	Shader(const std::string& vertexShaderFile, const std::string& fragmentShaderFile);
	explicit Shader(const std::string& computeShaderFile);
	~Shader();

	Shader(const Shader&) = delete;
	Shader& operator=(const Shader&) = delete;
	Shader(Shader&& other) noexcept;
	Shader& operator=(Shader&& other) noexcept;

	GLuint id() const { return m_id; }
	void use() const;

	// Uniform setters
	void setBool(const std::string& name, bool value) const;
	void setInt(const std::string& name, int value) const;
	void setFloat(const std::string& name, float value) const;
	void setFloatArray(const std::string& name, int count, const float* data) const;
	void setVec2(const std::string& name, const glm::vec2& value) const;
	void setVec2(const std::string& name, float x, float y) const;
	void setVec3(const std::string& name, const glm::vec3& value) const;
	void setVec3(const std::string& name, float x, float y, float z) const;
	void setVec4(const std::string& name, const glm::vec4& value) const;
	void setVec4(const std::string& name, float x, float y, float z, float w) const;
	void setMat2(const std::string& name, const glm::mat2& mat) const;
	void setMat3(const std::string& name, const glm::mat3& mat) const;
	void setMat4(const std::string& name, const glm::mat4& mat) const;

protected:
	GLuint m_id = 0;
	static inline const std::filesystem::path kShaderDir{"assets/shaders"};

	std::string readShaderFile(const std::string& fileName) const;
	std::string getShaderFilePath(const std::string& shaderFileName) const;
	void checkCompileErrors(GLuint shader, const std::string& type) const;

private:
	mutable std::unordered_map<std::string, GLint> m_uniformCache;

	GLint getUniformLocation(const std::string& name) const;
	GLuint compileShader(GLenum type, const char* source) const;
};
