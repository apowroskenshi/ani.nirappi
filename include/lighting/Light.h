#pragma once

#include <glm/glm.hpp>

class Shader;

class Light {
public:
	Light(const glm::vec3& color, float intensity)
		: m_color(color), m_intensity(intensity) {}

	virtual ~Light() = default;

	// Shadow matrices
	virtual glm::mat4 getViewMatrix(const glm::vec3& sceneMin, const glm::vec3& sceneMax) const = 0;
	virtual glm::mat4 getProjectionMatrix(const glm::vec3& sceneMin, const glm::vec3& sceneMax) const = 0;
	glm::mat4 getShadowMatrix(const glm::vec3& sceneMin, const glm::vec3& sceneMax) const;

	// Shader uniforms
	virtual void setLightingUniforms(Shader* shader) const = 0;
	void setShadowUniforms(Shader* shader, const glm::vec3& sceneMin, const glm::vec3& sceneMax) const;

	// Light properties
	virtual glm::vec3 getLightDirection() const = 0;
	virtual bool usesOrthographicShadows() const = 0;

	// Accessors
	const glm::vec3& color() const { return m_color; }
	float intensity() const { return m_intensity; }

	// Mutable refs for ImGui
	glm::vec3& color() { return m_color; }
	float& intensity() { return m_intensity; }

protected:
	glm::vec3 m_color;
	float m_intensity;
};
