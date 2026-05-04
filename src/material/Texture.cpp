#include "material/Texture.h"

#include <vector>
#include <cmath>
#include <algorithm>

#include <OpenImageIO/imageio.h>

// --- Constructors ---

Texture::Texture(const std::string& textureFileName, const std::filesystem::path& parentPath, const std::string& type)
	: m_type(type)
	, m_name(textureFileName)
{
	std::filesystem::path fullPath = parentPath / textureFileName;
	m_id = loadTexture(fullPath.string());
}

Texture::Texture(const std::string& textureFileName, const std::string& type)
	: m_type(type)
	, m_name(textureFileName)
{
	m_id = loadTexture((kTextureDir / textureFileName).string());
}

Texture::Texture(const std::string& type, int width, int height)
	: m_type(type)
{
	m_id = createComputeTarget(width, height);
}

Texture::Texture(const std::string& type, float r, float g, float b, float a)
	: m_type(type)
{
	m_id = loadSolidColor(r, g, b, a);
}

// --- RAII ---

Texture::~Texture()
{
	if (m_id != 0) {
		glDeleteTextures(1, &m_id);
	}
}

Texture::Texture(Texture&& other) noexcept
	: m_id(other.m_id)
	, m_name(std::move(other.m_name))
	, m_type(std::move(other.m_type))
{
	other.m_id = 0;
}

Texture& Texture::operator=(Texture&& other) noexcept
{
	if (this != &other) {
		if (m_id != 0) {
			glDeleteTextures(1, &m_id);
		}
		m_id = other.m_id;
		m_name = std::move(other.m_name);
		m_type = std::move(other.m_type);
		other.m_id = 0;
	}
	return *this;
}

// --- Texture creation helpers ---

GLuint Texture::loadSolidColor(float r, float g, float b, float a)
{
	GLuint textureID;
	glCreateTextures(GL_TEXTURE_2D, 1, &textureID);

	glTextureStorage2D(textureID, 1, GL_RGBA32F, 1, 1);

	float pixels[] = { r, g, b, a };
	glTextureSubImage2D(textureID, 0, 0, 0, 1, 1, GL_RGBA, GL_FLOAT, pixels);

	glTextureParameteri(textureID, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTextureParameteri(textureID, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glTextureParameteri(textureID, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTextureParameteri(textureID, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

	return textureID;
}

GLuint Texture::createComputeTarget(int width, int height)
{
	GLuint textureID;
	glCreateTextures(GL_TEXTURE_2D, 1, &textureID);

	glTextureStorage2D(textureID, 1, GL_RGBA32F, width, height);

	glTextureParameteri(textureID, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTextureParameteri(textureID, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTextureParameteri(textureID, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTextureParameteri(textureID, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	return textureID;
}

// --- Main texture loader ---

GLuint Texture::loadTexture(const std::string& textureFilePath)
{
	LOG_DEBUG << "Loading texture file: " << textureFilePath;

	auto in = OIIO::ImageInput::open(textureFilePath);
	if (!in) {
		LOG_ERROR << "Could not open file: " << OIIO::geterror();
		return 0;
	}

	const OIIO::ImageSpec& spec = in->spec();
	int width = spec.width;
	int height = spec.height;
	int channels = spec.nchannels;
	bool isHDR = (spec.format == OIIO::TypeDesc::FLOAT
		|| spec.format == OIIO::TypeDesc::HALF
		|| spec.get_string_attribute("oiio:ColorSpace") == "Linear");

	bool isColorMap = (m_type == "texture_diffuse" || m_type == "texture_emissive");

	// Determine GL formats based on channel count
	GLenum format;
	GLint internalFormat;

	if (channels == 1) {
		format = GL_RED;
		internalFormat = isHDR ? GL_R32F : GL_R8;
	}
	else if (channels == 3) {
		format = GL_RGB;
		if (isHDR)
			internalFormat = GL_RGB32F;
		else
			internalFormat = isColorMap ? GL_SRGB8 : GL_RGB8;
	}
	else if (channels == 4) {
		format = GL_RGBA;
		if (isHDR)
			internalFormat = GL_RGBA32F;
		else
			internalFormat = isColorMap ? GL_SRGB8_ALPHA8 : GL_RGBA8;
	}
	else {
		LOG_ERROR << "Unsupported number of channels: " << channels;
		in->close();
		return 0;
	}

	// Allocate immutable GPU storage
	GLuint textureID;
	glCreateTextures(GL_TEXTURE_2D, 1, &textureID);
	int levels = 1 + static_cast<int>(std::floor(std::log2(std::max(width, height))));
	glTextureStorage2D(textureID, levels, internalFormat, width, height);

	// Read image data with optional vertical flip
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	bool flipVertically = (m_type != "texture_envmap");
	auto scanlineBytes = static_cast<OIIO::stride_t>(width) * channels;

	if (isHDR) {
		scanlineBytes *= static_cast<OIIO::stride_t>(sizeof(float));
		std::vector<float> pixels(width * height * channels);
		void* startPtr = pixels.data();
		OIIO::stride_t ystride = OIIO::AutoStride;

		if (flipVertically) {
			startPtr = reinterpret_cast<char*>(pixels.data()) + static_cast<size_t>(height - 1) * scanlineBytes;
			ystride = -scanlineBytes;
		}

		if (in->read_image(0, 0, 0, channels, OIIO::TypeDesc::FLOAT, startPtr, OIIO::AutoStride, ystride)) {
			glTextureSubImage2D(textureID, 0, 0, 0, width, height, format, GL_FLOAT, pixels.data());
		}
	}
	else {
		std::vector<unsigned char> pixels(width * height * channels);
		void* startPtr = pixels.data();
		OIIO::stride_t ystride = OIIO::AutoStride;

		if (flipVertically) {
			startPtr = reinterpret_cast<char*>(pixels.data()) + static_cast<size_t>(height - 1) * scanlineBytes;
			ystride = -scanlineBytes;
		}

		if (in->read_image(0, 0, 0, channels, OIIO::TypeDesc::UINT8, startPtr, OIIO::AutoStride, ystride)) {
			glTextureSubImage2D(textureID, 0, 0, 0, width, height, format, GL_UNSIGNED_BYTE, pixels.data());
		}
	}

	// Configure sampling parameters and mipmaps
	if (m_type == "texture_envmap") {
		glGenerateTextureMipmap(textureID);
	}
	else if (m_type != "texture_irradiance" && m_type != "texture_skydome") {
		glTextureParameteri(textureID, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTextureParameteri(textureID, GL_TEXTURE_WRAP_T, GL_REPEAT);
		glTextureParameteri(textureID, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
		glTextureParameteri(textureID, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glGenerateTextureMipmap(textureID);
	}

	in->close();

	LOG_DEBUG << "Texture load complete: " << textureFilePath;

	return textureID;
}
