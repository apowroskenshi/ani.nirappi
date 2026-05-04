#include "lighting/HammersleyRandomGenerator.h"

HammersleyRandomGenerator::~HammersleyRandomGenerator()
{
	if (m_uboId != 0) glDeleteBuffers(1, &m_uboId);
}

float HammersleyRandomGenerator::radicalInverse(unsigned int bits)
{
	bits = (bits << 16u) | (bits >> 16u);
	bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
	bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
	bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
	bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
	return static_cast<float>(bits) * 2.3283064365386963e-10f;
}

void HammersleyRandomGenerator::generatePairs(int numSamples)
{
	m_alignedPairs.clear();
	m_alignedPairs.reserve(numSamples);
	for (int i = 0; i < numSamples; ++i) {
		float x = static_cast<float>(i) / static_cast<float>(numSamples);
		float y = radicalInverse(i);
		m_alignedPairs.emplace_back(x, y, 0.0f, 0.0f);
	}
}

void HammersleyRandomGenerator::initializeUBO()
{
	if (m_uboId == 0) glGenBuffers(1, &m_uboId);

	glBindBuffer(GL_UNIFORM_BUFFER, m_uboId);
	glBufferData(GL_UNIFORM_BUFFER,
		m_alignedPairs.size() * sizeof(glm::vec4),
		m_alignedPairs.data(),
		GL_DYNAMIC_DRAW);
}

void HammersleyRandomGenerator::updateUBO(int newNumSamples)
{
	generatePairs(newNumSamples);
	if (m_alignedPairs.empty()) return;

	if (m_uboId == 0) glGenBuffers(1, &m_uboId);

	glBindBuffer(GL_UNIFORM_BUFFER, m_uboId);
	glBufferData(GL_UNIFORM_BUFFER,
		m_alignedPairs.size() * sizeof(glm::vec4),
		m_alignedPairs.data(),
		GL_DYNAMIC_DRAW);
}

void HammersleyRandomGenerator::registerShader(GLuint shaderID)
{
	GLuint blockIndex = glGetUniformBlockIndex(shaderID, "HammersleyBlock");
	if (blockIndex != GL_INVALID_INDEX) {
		glUniformBlockBinding(shaderID, blockIndex, 0);
	}
}

void HammersleyRandomGenerator::bindUBOToShader(GLuint bindingIndex)
{
	glBindBufferBase(GL_UNIFORM_BUFFER, bindingIndex, m_uboId);
}
