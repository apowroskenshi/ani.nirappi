#include "lighting/SkyDome.h"
#include "renderer/Shader.h"

#include <vector>
#include <cmath>

#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>

SkyDome::SkyDome(unsigned int segments)
{
	setupSphere(segments);

	glGenSamplers(1, &m_samplerClamp);
	glSamplerParameteri(m_samplerClamp, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glSamplerParameteri(m_samplerClamp, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glSamplerParameteri(m_samplerClamp, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
}

SkyDome::~SkyDome()
{
	cleanup();
}

SkyDome::SkyDome(SkyDome&& other) noexcept
	: m_vao(other.m_vao)
	, m_vbo(other.m_vbo)
	, m_ebo(other.m_ebo)
	, m_samplerClamp(other.m_samplerClamp)
	, m_indexCount(other.m_indexCount)
{
	other.m_vao = 0;
	other.m_vbo = 0;
	other.m_ebo = 0;
	other.m_samplerClamp = 0;
	other.m_indexCount = 0;
}

SkyDome& SkyDome::operator=(SkyDome&& other) noexcept
{
	if (this != &other) {
		cleanup();

		m_vao = other.m_vao;
		m_vbo = other.m_vbo;
		m_ebo = other.m_ebo;
		m_samplerClamp = other.m_samplerClamp;
		m_indexCount = other.m_indexCount;

		other.m_vao = 0;
		other.m_vbo = 0;
		other.m_ebo = 0;
		other.m_samplerClamp = 0;
		other.m_indexCount = 0;
	}
	return *this;
}

void SkyDome::Draw(Shader* shader, GLuint envMapTexId) const
{
	glDepthFunc(GL_LEQUAL);

	shader->use();

	glBindTextureUnit(0, envMapTexId);
	glBindSampler(0, m_samplerClamp);
	shader->setInt("equirectangularMap", 0);

	glBindVertexArray(m_vao);
	glDrawElements(GL_TRIANGLES, m_indexCount, GL_UNSIGNED_INT, nullptr);
	glBindVertexArray(0);
	glBindSampler(0, 0);

	glDepthFunc(GL_LESS);
}

void SkyDome::setupSphere(unsigned int segments)
{
	std::vector<glm::vec3> positions;
	std::vector<unsigned int> indices;

	for (unsigned int y = 0; y <= segments; ++y) {
		for (unsigned int x = 0; x <= segments; ++x) {
			float xSeg = static_cast<float>(x) / static_cast<float>(segments);
			float ySeg = static_cast<float>(y) / static_cast<float>(segments);

			float xPos = std::cos(xSeg * 2.0f * glm::pi<float>()) * std::sin(ySeg * glm::pi<float>());
			float yPos = std::cos(ySeg * glm::pi<float>());
			float zPos = std::sin(xSeg * 2.0f * glm::pi<float>()) * std::sin(ySeg * glm::pi<float>());

			positions.emplace_back(xPos, yPos, zPos);
		}
	}

	for (unsigned int y = 0; y < segments; ++y) {
		for (unsigned int x = 0; x < segments; ++x) {
			unsigned int stride = segments + 1;
			indices.push_back((y + 1) * stride + x);
			indices.push_back(y * stride + x);
			indices.push_back(y * stride + (x + 1));

			indices.push_back((y + 1) * stride + x);
			indices.push_back(y * stride + (x + 1));
			indices.push_back((y + 1) * stride + (x + 1));
		}
	}

	m_indexCount = static_cast<unsigned int>(indices.size());

	glCreateVertexArrays(1, &m_vao);
	glCreateBuffers(1, &m_vbo);
	glCreateBuffers(1, &m_ebo);

	glNamedBufferStorage(m_vbo, positions.size() * sizeof(glm::vec3), positions.data(), 0);
	glNamedBufferStorage(m_ebo, indices.size() * sizeof(unsigned int), indices.data(), 0);

	glVertexArrayVertexBuffer(m_vao, 0, m_vbo, 0, sizeof(glm::vec3));
	glVertexArrayElementBuffer(m_vao, m_ebo);

	glEnableVertexArrayAttrib(m_vao, 0);
	glVertexArrayAttribFormat(m_vao, 0, 3, GL_FLOAT, GL_FALSE, 0);
	glVertexArrayAttribBinding(m_vao, 0, 0);
}

void SkyDome::cleanup()
{
	if (m_samplerClamp != 0) glDeleteSamplers(1, &m_samplerClamp);
	if (m_vao != 0) glDeleteVertexArrays(1, &m_vao);
	if (m_vbo != 0) glDeleteBuffers(1, &m_vbo);
	if (m_ebo != 0) glDeleteBuffers(1, &m_ebo);
	m_samplerClamp = 0;
	m_vao = 0;
	m_vbo = 0;
	m_ebo = 0;
}
