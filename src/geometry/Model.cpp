#include "geometry/Model.h"
#include "geometry/ModelLoader.h"

#include <filesystem>

#include <glm/gtc/matrix_transform.hpp>

#include "core/Logger.h"

Model::Model(const std::string& modelFileName, glm::vec3 position, float rotateY, float scale)
	: m_position(position)
	, m_scale(scale)
	, m_rotateY(rotateY)
	, m_modelName(std::filesystem::path(modelFileName).stem().string())
{
	ModelLoader loader;
	LoadedModel data = loader.load(modelFileName);

	m_meshes = std::move(data.meshes);
	m_sceneMin = data.sceneMin;
	m_sceneMax = data.sceneMax;
}

void Model::Draw(Shader* shader)
{
	glm::mat4 model = glm::mat4(1.0f);
	model = glm::translate(model, m_position);
	model = glm::rotate(model, glm::radians(m_rotateY), glm::vec3(0.0f, 1.0f, 0.0f));
	model = glm::scale(model, glm::vec3(m_scale));

	shader->setMat4("model", model);

	for (auto& mesh : m_meshes) {
		mesh.Draw(shader);
	}
}
