#pragma once

#include <vector>

#include <glad/glad.h>
#include <glm/glm.hpp>

class HammersleyRandomGenerator {
public:
	HammersleyRandomGenerator() = default;
	~HammersleyRandomGenerator();

	HammersleyRandomGenerator(const HammersleyRandomGenerator&) = delete;
	HammersleyRandomGenerator& operator=(const HammersleyRandomGenerator&) = delete;

	void generatePairs(int numSamples);
	void initializeUBO();
	void updateUBO(int newNumSamples);
	void registerShader(GLuint shaderID);
	void bindUBOToShader(GLuint bindingIndex = 0);

private:
	static float radicalInverse(unsigned int bits);

	GLuint m_uboId = 0;
	std::vector<glm::vec4> m_alignedPairs;
};
