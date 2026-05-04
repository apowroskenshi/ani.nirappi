#pragma once

#include <vector>

#include <glad/glad.h>

#include "core/Logger.h"

class FrameBuffer {
public:
	FrameBuffer() = default;
	~FrameBuffer();

	FrameBuffer(const FrameBuffer&) = delete;
	FrameBuffer& operator=(const FrameBuffer&) = delete;
	FrameBuffer(FrameBuffer&& other) noexcept;
	FrameBuffer& operator=(FrameBuffer&& other) noexcept;

	bool init(unsigned int width, unsigned int height,
		unsigned int numColorAttachments = 1, bool enableDepthAttachment = false);
	bool initShadowFBO(unsigned int width, unsigned int height);

	void bind() const;
	void unbind() const;
	void persistDepthInfo() const;

	GLuint getTextureID(unsigned int index = 0) const;
	unsigned int getWidth() const { return m_width; }
	unsigned int getHeight() const { return m_height; }
	GLuint getFBOID() const { return m_fboId; }

private:
	void cleanup();

	GLuint m_fboId = 0;
	GLuint m_rboId = 0;
	std::vector<GLuint> m_colorTextures;

	unsigned int m_width = 0;
	unsigned int m_height = 0;
};
