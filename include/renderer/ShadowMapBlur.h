#pragma once

#include <vector>
#include <memory>

#include <glad/glad.h>

#include "renderer/BlurShader.h"
#include "material/Texture.h"
#include "renderer/FrameBuffer.h"

class ShadowMapBlur {
public:
	ShadowMapBlur(int blurWidth = 2, int shadowMapSize = 2048);
	~ShadowMapBlur();

	ShadowMapBlur(const ShadowMapBlur&) = delete;
	ShadowMapBlur& operator=(const ShadowMapBlur&) = delete;

	void performBlur(FrameBuffer* shadowFBO);
	void updateBlur(int newWidth);
	GLuint getBlurredShadowMapTexID() const { return m_blurredShadowMap->id(); }

private:
	void initBuffers();
	void computeWeights();

	int m_blurWidth;
	std::vector<float> m_weights;
	GLuint m_weightBuf = 0;

	std::unique_ptr<BlurShader> m_horizontalBlur;
	std::unique_ptr<BlurShader> m_verticalBlur;
	std::unique_ptr<Texture> m_tempShadowMap;
	std::unique_ptr<Texture> m_blurredShadowMap;
};
