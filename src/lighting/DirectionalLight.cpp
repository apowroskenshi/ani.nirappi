#include "lighting/DirectionalLight.h"
#include "renderer/Shader.h"

#include <glm/gtc/matrix_transform.hpp>

DirectionalLight::DirectionalLight(const glm::vec3& direction, const glm::vec3& color, float intensity)
	: Light(color, intensity)
	, m_direction(glm::normalize(direction))
	, m_position(-m_direction)
{
}

void DirectionalLight::setDirection(const glm::vec3& dir)
{
	m_direction = glm::normalize(dir);
	m_position = -m_direction;
}

glm::mat4 DirectionalLight::getViewMatrix(const glm::vec3& sceneMin, const glm::vec3& sceneMax) const
{
	glm::vec3 center = (sceneMin + sceneMax) * 0.5f;
	float radius = glm::length(sceneMax - sceneMin) * 0.5f;

	glm::vec3 dir = glm::normalize(m_direction);
	glm::vec3 lightPos = center - dir * radius;

	glm::vec3 up = abs(glm::dot(dir, glm::vec3(0, 1, 0))) > 0.99f
		? glm::vec3(0, 0, 1)
		: glm::vec3(0, 1, 0);

	return glm::lookAt(lightPos, center, up);
}

glm::mat4 DirectionalLight::getProjectionMatrix(const glm::vec3& sceneMin, const glm::vec3& sceneMax) const
{
	glm::vec3 corners[8] = {
		{sceneMin.x, sceneMin.y, sceneMin.z}, {sceneMax.x, sceneMin.y, sceneMin.z},
		{sceneMin.x, sceneMax.y, sceneMin.z}, {sceneMax.x, sceneMax.y, sceneMin.z},
		{sceneMin.x, sceneMin.y, sceneMax.z}, {sceneMax.x, sceneMin.y, sceneMax.z},
		{sceneMin.x, sceneMax.y, sceneMax.z}, {sceneMax.x, sceneMax.y, sceneMax.z}
	};

	glm::mat4 view = getViewMatrix(sceneMin, sceneMax);
	glm::vec3 lightMin(FLT_MAX), lightMax(-FLT_MAX);

	for (int i = 0; i < 8; i++) {
		glm::vec3 lc = glm::vec3(view * glm::vec4(corners[i], 1.0f));
		lightMin = glm::min(lightMin, lc);
		lightMax = glm::max(lightMax, lc);
	}

	return glm::ortho(lightMin.x, lightMax.x,
		lightMin.y, lightMax.y,
		-lightMax.z, -lightMin.z);
}

void DirectionalLight::setLightingUniforms(Shader* shader) const
{
	shader->setVec3("light.position", -m_direction);
	shader->setVec3("light.color", m_color);
	shader->setFloat("light.intensity", m_intensity);
}
