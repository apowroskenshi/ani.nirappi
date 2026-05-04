#pragma once

#include <string>
#include <vector>
#include <memory>

#include <glad/glad.h>
#include <glm/glm.hpp>

#include "renderer/Shader.h"
#include "material/Texture.h"

struct Vertex {
	glm::vec3 position;
	glm::vec3 normal;
	glm::vec2 texCoords;
	glm::vec3 tangent;
	glm::vec3 biTangent;
};

struct Material {
	glm::vec3 ambient{0.0f};
	glm::vec3 diffuse{0.5f};
	glm::vec3 specular{0.04f};
	float opacity = 1.0f;
	float metallic = 0.0f;
	float roughness = 0.5f;

	bool hasDiffuseMap = false;
	bool hasNormalMap = false;
	bool hasRoughnessMap = false;
	bool hasMetallicMap = false;
};

class Mesh {
public:
	Mesh(std::vector<Vertex> vertices,
		std::vector<unsigned int> indices,
		std::vector<std::shared_ptr<Texture>> textures,
		Material material);

	~Mesh();

	Mesh(const Mesh&) = delete;
	Mesh& operator=(const Mesh&) = delete;
	Mesh(Mesh&& other) noexcept;
	Mesh& operator=(Mesh&& other) noexcept;

	void Draw(Shader* shader);

private:
	void setupMesh();

	GLuint m_vao = 0;
	GLuint m_vbo = 0;
	GLuint m_ebo = 0;

	std::vector<Vertex> m_vertices;
	std::vector<unsigned int> m_indices;
	std::vector<std::shared_ptr<Texture>> m_textures;
	Material m_material;
};
