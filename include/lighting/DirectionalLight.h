#pragma once

#include "lighting/Light.h"

class DirectionalLight : public Light {
public:
	DirectionalLight(const glm::vec3& direction = glm::vec3(-2.0f, -5.0f, -1.0f),
		const glm::vec3& color = glm::vec3(1.0f),
		float intensity = 10.0f);

	void setDirection(const glm::vec3& dir);

	// Light interface
	glm::mat4 getViewMatrix(const glm::vec3& sceneMin, const glm::vec3& sceneMax) const override;
	glm::mat4 getProjectionMatrix(const glm::vec3& sceneMin, const glm::vec3& sceneMax) const override;
	void setLightingUniforms(Shader* shader) const override;
	glm::vec3 getLightDirection() const override { return m_direction; }
	bool usesOrthographicShadows() const override { return true; }

	// Accessors
	const glm::vec3& direction() const { return m_direction; }
	glm::vec3& direction() { return m_direction; }

private:
	glm::vec3 m_direction;
	glm::vec3 m_position;
};
