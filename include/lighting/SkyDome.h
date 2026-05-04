#pragma once

#include <glad/glad.h>

class Shader;

class SkyDome {
public:
	SkyDome(unsigned int segments = 64);
	~SkyDome();

	SkyDome(const SkyDome&) = delete;
	SkyDome& operator=(const SkyDome&) = delete;
	SkyDome(SkyDome&& other) noexcept;
	SkyDome& operator=(SkyDome&& other) noexcept;

	void Draw(Shader* shader, GLuint envMapTexId) const;

private:
	void setupSphere(unsigned int segments);
	void cleanup();

	GLuint m_vao = 0;
	GLuint m_vbo = 0;
	GLuint m_ebo = 0;
	GLuint m_samplerClamp = 0;
	unsigned int m_indexCount = 0;
};
