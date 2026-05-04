#include "lighting/Light.h"
#include "renderer/Shader.h"

#include <glm/gtc/matrix_transform.hpp>

glm::mat4 Light::getShadowMatrix(const glm::vec3& sceneMin, const glm::vec3& sceneMax) const
{
	glm::mat4 view = getViewMatrix(sceneMin, sceneMax);
	glm::mat4 projection = getProjectionMatrix(sceneMin, sceneMax);

	// Transform to [0,1] texture space
	glm::mat4 textureMatrix = glm::translate(glm::mat4(1.0f), glm::vec3(0.5f))
		* glm::scale(glm::mat4(1.0f), glm::vec3(0.5f));

	return textureMatrix * projection * view;
}

void Light::setShadowUniforms(Shader* shader, const glm::vec3& sceneMin, const glm::vec3& sceneMax) const
{
	glm::mat4 shadowMatrix = getShadowMatrix(sceneMin, sceneMax);
	shader->setMat4("shadowMatrix", shadowMatrix);
	shader->setBool("useOrthographicShadows", usesOrthographicShadows());
}
