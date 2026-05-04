#include "lighting/SHIrradiance.h"
#include "renderer/Shader.h"
#include "core/Logger.h"

#include <vector>
#include <cmath>
#include <algorithm>
#include <filesystem>

#include <OpenImageIO/imageio.h>
#include <fmt/format.h>
#include <glm/gtc/constants.hpp>

bool SHIrradiance::computeFromHDR(const std::string& fileName)
{
	static const std::filesystem::path kTextureDir{"assets/textures"};
	std::string fullPath = (kTextureDir / fileName).string();

	auto inp = OIIO::ImageInput::open(fullPath);
	if (!inp) {
		LOG_ERROR << "[SHIrradiance] Cannot open: " << fullPath;
		return false;
	}

	const OIIO::ImageSpec& spec = inp->spec();
	int W = spec.width;
	int H = spec.height;
	int C = spec.nchannels;

	std::vector<float> pixels(W * H * C);
	inp->read_image(0, 0, 0, C, OIIO::TypeDesc::FLOAT, pixels.data());
	inp->close();

	std::array<glm::vec3, 9> Llm{};

	constexpr double pi = glm::pi<double>();
	const double dTheta = pi / H;
	const double dPhi = 2.0 * pi / W;
	double areaTest = 0.0;

	for (int j = 0; j < H; ++j) {
		double theta = pi * (j + 0.5) / H;
		double sinTheta = std::sin(theta);
		double cosTheta = std::cos(theta);

		for (int i = 0; i < W; ++i) {
			double u = (i + 0.5) / double(W);
			double phi = (0.5 - u) * 2.0 * pi;

			float x = static_cast<float>(sinTheta * std::sin(phi));
			float y = static_cast<float>(cosTheta);
			float z = static_cast<float>(sinTheta * std::cos(phi));

			int base = (j * W + i) * C;
			glm::vec3 L(
				std::max(0.0f, pixels[base + 0]),
				std::max(0.0f, pixels[base + 1]),
				std::max(0.0f, C >= 3 ? pixels[base + 2] : pixels[base + 0])
			);

			float Y[9];
			evalBasis(x, y, z, Y);

			float weight = static_cast<float>(sinTheta * dTheta * dPhi);
			for (int k = 0; k < 9; ++k)
				Llm[k] += L * Y[k] * weight;

			areaTest += sinTheta * dTheta * dPhi;
		}
	}

	if (std::abs(areaTest - 4.0 * pi) > 0.1)
		LOG_ERROR << "[SHIrradiance] Area test = " << areaTest << " (expected ~12.566)";
	else
		LOG_DEBUG << "[SHIrradiance] Area test OK: " << areaTest;

	// Elm = Ahat_l * Llm
	const float Ahat[9] = {
		static_cast<float>(pi),
		static_cast<float>((2.0 / 3.0) * pi), static_cast<float>((2.0 / 3.0) * pi), static_cast<float>((2.0 / 3.0) * pi),
		static_cast<float>((1.0 / 4.0) * pi), static_cast<float>((1.0 / 4.0) * pi), static_cast<float>((1.0 / 4.0) * pi),
		static_cast<float>((1.0 / 4.0) * pi), static_cast<float>((1.0 / 4.0) * pi),
	};

	std::array<glm::vec3, 9> coeffs{};
	for (int k = 0; k < 9; ++k)
		coeffs[k] = Llm[k] * Ahat[k];

	EnvMap envMap;
	envMap.coeffs = coeffs;
	envMap.tex = std::make_unique<Texture>(fileName, "texture_envmap");

	m_coeffMap[m_totalMaps] = std::move(envMap);
	++m_totalMaps;

	m_valid = true;
	print(coeffs);
	return true;
}

void SHIrradiance::uploadToShader(Shader* shader) const
{
	if (!m_valid) return;

	const auto& coeffs = m_coeffMap.at(currMapIdx).coeffs;

	for (int k = 0; k < 9; ++k) {
		std::string name = "shCoeffs[" + std::to_string(k) + "]";
		shader->setVec3(name, coeffs[k]);
	}
}

unsigned int SHIrradiance::getCurrentEnvMapTexId() const
{
	return m_coeffMap.at(currMapIdx).tex->id();
}

glm::vec3 SHIrradiance::getL1SunDirection() const
{
	const auto& coeffs = m_coeffMap.at(currMapIdx).coeffs;

	glm::vec3 L1;
	L1.x = 0.2126f * coeffs[3].r + 0.7152f * coeffs[3].g + 0.0722f * coeffs[3].b;
	L1.y = 0.2126f * coeffs[1].r + 0.7152f * coeffs[1].g + 0.0722f * coeffs[1].b;
	L1.z = 0.2126f * coeffs[2].r + 0.7152f * coeffs[2].g + 0.0722f * coeffs[2].b;

	return glm::normalize(L1);
}

glm::vec3 SHIrradiance::getSunColor() const
{
	const auto& coeffs = m_coeffMap.at(currMapIdx).coeffs;
	glm::vec3 sunDir = getL1SunDirection();

	float Y[9];
	evalBasis(sunDir.x, sunDir.y, sunDir.z, Y);

	glm::vec3 radiance(0.0f);
	for (int k = 0; k < 9; k++)
		radiance += coeffs[k] * Y[k];

	return glm::max(radiance, glm::vec3(0.0f));
}

void SHIrradiance::print(const std::array<glm::vec3, 9>& coeffs) const
{
	const char* names[9] = {
		"L00 ", "L1-1", "L10 ", "L11 ",
		"L2-2", "L2-1", "L20 ", "L21 ", "L22 "
	};
	LOG_DEBUG << "SH irradiance coefficients:";
	for (int k = 0; k < 9; ++k) {
		LOG_DEBUG << fmt::format("  E{} = ({}, {}, {})", names[k], coeffs[k].r, coeffs[k].g, coeffs[k].b);
	}
}

void SHIrradiance::evalBasis(float x, float y, float z, float Y[9])
{
	constexpr float pi = glm::pi<float>();

	Y[0] = 0.5f * std::sqrt(1.0f / pi);
	Y[1] = 0.5f * std::sqrt(3.0f / pi) * y;
	Y[2] = 0.5f * std::sqrt(3.0f / pi) * z;
	Y[3] = 0.5f * std::sqrt(3.0f / pi) * x;
	Y[4] = 0.5f * std::sqrt(15.0f / pi) * x * y;
	Y[5] = 0.5f * std::sqrt(15.0f / pi) * y * z;
	Y[6] = 0.25f * std::sqrt(5.0f / pi) * (3.0f * z * z - 1.0f);
	Y[7] = 0.5f * std::sqrt(15.0f / pi) * x * z;
	Y[8] = 0.25f * std::sqrt(15.0f / pi) * (x * x - y * y);
}
