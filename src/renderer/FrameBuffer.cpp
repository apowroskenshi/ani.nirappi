#include "renderer/FrameBuffer.h"

FrameBuffer::~FrameBuffer()
{
	cleanup();
}

FrameBuffer::FrameBuffer(FrameBuffer&& other) noexcept
	: m_fboId(other.m_fboId)
	, m_rboId(other.m_rboId)
	, m_colorTextures(std::move(other.m_colorTextures))
	, m_width(other.m_width)
	, m_height(other.m_height)
{
	other.m_fboId = 0;
	other.m_rboId = 0;
	other.m_width = 0;
	other.m_height = 0;
}

FrameBuffer& FrameBuffer::operator=(FrameBuffer&& other) noexcept
{
	if (this != &other) {
		cleanup();

		m_fboId = other.m_fboId;
		m_rboId = other.m_rboId;
		m_colorTextures = std::move(other.m_colorTextures);
		m_width = other.m_width;
		m_height = other.m_height;

		other.m_fboId = 0;
		other.m_rboId = 0;
		other.m_width = 0;
		other.m_height = 0;
	}
	return *this;
}

bool FrameBuffer::init(unsigned int width, unsigned int height,
	unsigned int numColorAttachments, bool enableDepthAttachment)
{
	m_width = width;
	m_height = height;

	glGenFramebuffers(1, &m_fboId);
	glBindFramebuffer(GL_FRAMEBUFFER, m_fboId);

	// Create and attach color textures
	m_colorTextures.resize(numColorAttachments);
	glGenTextures(static_cast<GLsizei>(numColorAttachments), m_colorTextures.data());

	std::vector<GLuint> drawBuffers;
	for (unsigned int i = 0; i < numColorAttachments; ++i) {
		glBindTexture(GL_TEXTURE_2D, m_colorTextures[i]);

		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, width, height, 0, GL_RGBA, GL_FLOAT, nullptr);

		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + i, GL_TEXTURE_2D, m_colorTextures[i], 0);
		drawBuffers.push_back(GL_COLOR_ATTACHMENT0 + i);
	}

	if (!drawBuffers.empty()) {
		glDrawBuffers(static_cast<GLsizei>(drawBuffers.size()), drawBuffers.data());
	}

	if (enableDepthAttachment) {
		glGenRenderbuffers(1, &m_rboId);
		glBindRenderbuffer(GL_RENDERBUFFER, m_rboId);
		glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width, height);
		glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, m_rboId);
	}

	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
		LOG_ERROR << "FrameBuffer not complete: 0x" << std::hex << glCheckFramebufferStatus(GL_FRAMEBUFFER);
		cleanup();
		return false;
	}

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glBindTexture(GL_TEXTURE_2D, 0);
	return true;
}

bool FrameBuffer::initShadowFBO(unsigned int width, unsigned int height)
{
	m_width = width;
	m_height = height;

	glGenFramebuffers(1, &m_fboId);
	glBindFramebuffer(GL_FRAMEBUFFER, m_fboId);

	// Single color texture for moment shadow storage
	m_colorTextures.resize(1);
	glGenTextures(1, m_colorTextures.data());
	glBindTexture(GL_TEXTURE_2D, m_colorTextures[0]);

	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, width, height, 0, GL_RGBA, GL_FLOAT, nullptr);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_colorTextures[0], 0);

	GLuint drawBuffer = GL_COLOR_ATTACHMENT0;
	glDrawBuffers(1, &drawBuffer);

	// Depth-only renderbuffer
	glGenRenderbuffers(1, &m_rboId);
	glBindRenderbuffer(GL_RENDERBUFFER, m_rboId);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT32F, width, height);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, m_rboId);

	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
		LOG_ERROR << "Shadow FBO not complete: 0x" << std::hex << glCheckFramebufferStatus(GL_FRAMEBUFFER);
		cleanup();
		return false;
	}

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glBindTexture(GL_TEXTURE_2D, 0);
	return true;
}

void FrameBuffer::bind() const
{
	glBindFramebuffer(GL_FRAMEBUFFER, m_fboId);
	glViewport(0, 0, m_width, m_height);
}

void FrameBuffer::unbind() const
{
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void FrameBuffer::persistDepthInfo() const
{
	glBindFramebuffer(GL_READ_FRAMEBUFFER, m_fboId);
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
	glBlitFramebuffer(0, 0, m_width, m_height, 0, 0, m_width, m_height, GL_DEPTH_BUFFER_BIT, GL_NEAREST);
	glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
}

GLuint FrameBuffer::getTextureID(unsigned int index) const
{
	if (index < m_colorTextures.size()) {
		return m_colorTextures[index];
	}
	LOG_ERROR << "Invalid texture index: " << index;
	return 0;
}

void FrameBuffer::cleanup()
{
	if (m_fboId != 0) glDeleteFramebuffers(1, &m_fboId);
	if (m_rboId != 0) glDeleteRenderbuffers(1, &m_rboId);
	if (!m_colorTextures.empty()) {
		glDeleteTextures(static_cast<GLsizei>(m_colorTextures.size()), m_colorTextures.data());
		m_colorTextures.clear();
	}
	m_fboId = 0;
	m_rboId = 0;
}
