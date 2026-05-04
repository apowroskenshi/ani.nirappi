#pragma once

#include <string>
#include <filesystem>

#include <glad/glad.h>

#include "core/Logger.h"

class Texture {
public:
	Texture() = default;
	Texture(const std::string& textureFileName, const std::filesystem::path& parentPath, const std::string& type);
	Texture(const std::string& textureFileName, const std::string& type);
	Texture(const std::string& type, int width, int height);
	Texture(const std::string& type, float r, float g, float b, float a = 1.0f);
	~Texture();

	Texture(const Texture&) = delete;
	Texture& operator=(const Texture&) = delete;
	Texture(Texture&& other) noexcept;
	Texture& operator=(Texture&& other) noexcept;

	GLuint id() const { return m_id; }
	const std::string& name() const { return m_name; }
	const std::string& type() const { return m_type; }

private:
	static inline const std::filesystem::path kTextureDir{"assets/textures"};

	GLuint m_id = 0;
	std::string m_name;
	std::string m_type;

	GLuint loadTexture(const std::string& textureFilePath);
	GLuint loadSolidColor(float r, float g, float b, float a);
	GLuint createComputeTarget(int width, int height);
};
