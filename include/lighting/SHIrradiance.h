#pragma once

#include <array>
#include <string>
#include <memory>
#include <unordered_map>

#include <glm/glm.hpp>

#include "material/Texture.h"

class Shader;

struct EnvMap {
	std::array<glm::vec3, 9> coeffs{};
	std::unique_ptr<Texture> tex;
};

class SHIrradiance {
public:
	bool computeFromHDR(const std::string& fileName);
	void uploadToShader(Shader* shader) const;

	unsigned int getCurrentEnvMapTexId() const;
	glm::vec3 getL1SunDirection() const;
	glm::vec3 getSunColor() const;

	int currMapIdx = 0;

private:
	static void evalBasis(float x, float y, float z, float Y[9]);
	void print(const std::array<glm::vec3, 9>& coeffs) const;

	std::unordered_map<int, EnvMap> m_coeffMap;
	int m_totalMaps = 0;
	bool m_valid = false;
};
