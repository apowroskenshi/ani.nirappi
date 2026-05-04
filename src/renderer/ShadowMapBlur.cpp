#include "renderer/ShadowMapBlur.h"
#include "core/Logger.h"

#include <cmath>

ShadowMapBlur::ShadowMapBlur(int blurWidth, int shadowMapSize)
	: m_blurWidth(blurWidth)
	, m_horizontalBlur(std::make_unique<BlurShader>("shadowblur.comp", true))
	, m_verticalBlur(std::make_unique<BlurShader>("shadowblur.comp", false))
	, m_tempShadowMap(std::make_unique<Texture>("compute_target", shadowMapSize, shadowMapSize))
	, m_blurredShadowMap(std::make_unique<Texture>("compute_target", shadowMapSize, shadowMapSize))
{
	computeWeights();
	initBuffers();
}

ShadowMapBlur::~ShadowMapBlur()
{
	if (m_weightBuf != 0) glDeleteBuffers(1, &m_weightBuf);
}

void ShadowMapBlur::computeWeights()
{
	m_weights.clear();
	float s = static_cast<float>(m_blurWidth) / 2.0f;
	float sum = 0.0f;

	for (int i = -m_blurWidth; i <= m_blurWidth; i++) {
		float ex = -0.5f * pow(static_cast<float>(i) / s, 2.0f);
		float kw = exp(ex);
		m_weights.push_back(kw);
		sum += kw;
	}

	for (float& w : m_weights) {
		w /= sum;
	}
}

void ShadowMapBlur::initBuffers()
{
	glGenBuffers(1, &m_weightBuf);
	GLuint maxBuffSize = 101 * sizeof(float);

	glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_weightBuf);
	glBufferData(GL_SHADER_STORAGE_BUFFER, maxBuffSize, m_weights.data(), GL_STATIC_DRAW);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, m_weightBuf);
}

void ShadowMapBlur::updateBlur(int newWidth)
{
	if (newWidth < 1) newWidth = 1;
	m_blurWidth = newWidth;

	computeWeights();

	glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_weightBuf);
	glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, m_weights.size() * sizeof(float), m_weights.data());
}

void ShadowMapBlur::performBlur(FrameBuffer* shadowFBO)
{
	GLuint srcImgUnit = 0, tmpImgUnit = 1, dstImgUnit = 2;

	glBindImageTexture(srcImgUnit, shadowFBO->getTextureID(), 0, GL_FALSE, 0, GL_READ_ONLY, GL_RGBA32F);
	glBindImageTexture(tmpImgUnit, m_tempShadowMap->id(), 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA32F);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, m_weightBuf);

	// Horizontal pass
	m_horizontalBlur->use();
	m_horizontalBlur->setInt("src", static_cast<int>(srcImgUnit));
	m_horizontalBlur->setInt("dst", static_cast<int>(tmpImgUnit));
	m_horizontalBlur->setInt("blurWidth", 2 * m_blurWidth);

	int width = shadowFBO->getWidth();
	int height = shadowFBO->getHeight();

	glDispatchCompute(width / 128, height, 1);
	glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

	glBindImageTexture(tmpImgUnit, m_tempShadowMap->id(), 0, GL_FALSE, 0, GL_READ_ONLY, GL_RGBA32F);
	glBindImageTexture(dstImgUnit, m_blurredShadowMap->id(), 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA32F);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, m_weightBuf);

	// Vertical pass
	m_verticalBlur->use();
	m_verticalBlur->setInt("src", static_cast<int>(tmpImgUnit));
	m_verticalBlur->setInt("dst", static_cast<int>(dstImgUnit));
	m_verticalBlur->setInt("blurWidth", 2 * m_blurWidth);

	glDispatchCompute(width, height / 128, 1);
	glMemoryBarrier(GL_TEXTURE_FETCH_BARRIER_BIT);
}
