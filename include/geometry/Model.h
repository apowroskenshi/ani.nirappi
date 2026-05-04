#pragma once

#include <string>
#include <vector>

#include <glm/glm.hpp>

#include "geometry/Mesh.h"
#include "renderer/Shader.h"

class Model {
public:
	Model() = default;
	Model(const std::string& modelFileName, glm::vec3 position, float rotateY = 0.0f, float scale = 1.0f);
	~Model() = default;

	Model(const Model&) = delete;
	Model& operator=(const Model&) = delete;
	Model(Model&&) noexcept = default;
	Model& operator=(Model&&) noexcept = default;

	void Draw(Shader* shader);

	// Mutable refs for ImGui sliders
	glm::vec3& position() { return m_position; }
	float& scale() { return m_scale; }
	float& rotateY() { return m_rotateY; }

	// Read-only accessors
	const glm::vec3& position() const { return m_position; }
	float scale() const { return m_scale; }
	float rotateY() const { return m_rotateY; }
	const glm::vec3& sceneMin() const { return m_sceneMin; }
	const glm::vec3& sceneMax() const { return m_sceneMax; }
	const std::string& modelName() const { return m_modelName; }

private:
	std::vector<Mesh> m_meshes;
	std::string m_modelName;

	// Transform
	glm::vec3 m_position{0.0f};
	float m_scale = 1.0f;
	float m_rotateY = 0.0f;

	// Bounding box
	glm::vec3 m_sceneMin{FLT_MAX};
	glm::vec3 m_sceneMax{-FLT_MAX};
};
