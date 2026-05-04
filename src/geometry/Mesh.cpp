#include "geometry/Mesh.h"

// --- Constructor ---

Mesh::Mesh(std::vector<Vertex> vertices,
	std::vector<unsigned int> indices,
	std::vector<std::shared_ptr<Texture>> textures,
	Material material)
	: m_vertices(std::move(vertices))
	, m_indices(std::move(indices))
	, m_textures(std::move(textures))
	, m_material(std::move(material))
{
	setupMesh();
}

// --- RAII ---

Mesh::~Mesh()
{
	if (m_vao != 0) {
		glDeleteVertexArrays(1, &m_vao);
		glDeleteBuffers(1, &m_vbo);
		glDeleteBuffers(1, &m_ebo);
	}
}

Mesh::Mesh(Mesh&& other) noexcept
	: m_vao(other.m_vao)
	, m_vbo(other.m_vbo)
	, m_ebo(other.m_ebo)
	, m_vertices(std::move(other.m_vertices))
	, m_indices(std::move(other.m_indices))
	, m_textures(std::move(other.m_textures))
	, m_material(std::move(other.m_material))
{
	other.m_vao = 0;
	other.m_vbo = 0;
	other.m_ebo = 0;
}

Mesh& Mesh::operator=(Mesh&& other) noexcept
{
	if (this != &other) {
		if (m_vao != 0) {
			glDeleteVertexArrays(1, &m_vao);
			glDeleteBuffers(1, &m_vbo);
			glDeleteBuffers(1, &m_ebo);
		}

		m_vao = other.m_vao;
		m_vbo = other.m_vbo;
		m_ebo = other.m_ebo;
		m_vertices = std::move(other.m_vertices);
		m_indices = std::move(other.m_indices);
		m_textures = std::move(other.m_textures);
		m_material = std::move(other.m_material);

		other.m_vao = 0;
		other.m_vbo = 0;
		other.m_ebo = 0;
	}
	return *this;
}

// --- Rendering ---

void Mesh::Draw(Shader* shader)
{
	constexpr int kDiffuseSlot   = 0;
	constexpr int kNormalSlot    = 1;
	constexpr int kRoughnessSlot = 2;
	constexpr int kMetallicSlot  = 3;
	constexpr int kAoSlot        = 4;

	for (const auto& texture : m_textures) {
		int slot = -1;
		const std::string& type = texture->type();

		if (type == "texture_diffuse")         slot = kDiffuseSlot;
		else if (type == "texture_normal")     slot = kNormalSlot;
		else if (type == "texture_roughness")  slot = kRoughnessSlot;
		else if (type == "texture_metallic")   slot = kMetallicSlot;
		else if (type == "texture_ao")         slot = kAoSlot;

		if (slot >= 0) {
			glActiveTexture(GL_TEXTURE0 + slot);
			shader->setInt(type, slot);
			glBindTexture(GL_TEXTURE_2D, texture->id());
		}
	}

	shader->setVec3("material.ambient", m_material.ambient);
	shader->setVec3("material.diffuse", m_material.diffuse);
	shader->setVec3("material.specular", m_material.specular);
	shader->setFloat("material.metallic", m_material.metallic);
	shader->setFloat("material.roughness", m_material.roughness);
	shader->setFloat("material.opacity", m_material.opacity);

	shader->setBool("material.hasDiffuseMap", m_material.hasDiffuseMap);
	shader->setBool("material.hasNormalMap", m_material.hasNormalMap);
	shader->setBool("material.hasRoughnessMap", m_material.hasRoughnessMap);
	shader->setBool("material.hasMetallicMap", m_material.hasMetallicMap);

	glBindVertexArray(m_vao);
	glDrawElements(GL_TRIANGLES, static_cast<unsigned int>(m_indices.size()), GL_UNSIGNED_INT, nullptr);
	glBindVertexArray(0);

	glActiveTexture(GL_TEXTURE0);
}

// --- GL setup ---

void Mesh::setupMesh()
{
	glGenVertexArrays(1, &m_vao);
	glGenBuffers(1, &m_vbo);
	glGenBuffers(1, &m_ebo);

	glBindVertexArray(m_vao);

	glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
	glBufferData(GL_ARRAY_BUFFER,
		m_vertices.size() * sizeof(Vertex),
		m_vertices.data(), GL_STATIC_DRAW);

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ebo);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER,
		m_indices.size() * sizeof(unsigned int),
		m_indices.data(), GL_STATIC_DRAW);

	// Position
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
		reinterpret_cast<void*>(offsetof(Vertex, position)));

	// Normal
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
		reinterpret_cast<void*>(offsetof(Vertex, normal)));

	// Texture coordinates
	glEnableVertexAttribArray(2);
	glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex),
		reinterpret_cast<void*>(offsetof(Vertex, texCoords)));

	// Tangent
	glEnableVertexAttribArray(3);
	glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
		reinterpret_cast<void*>(offsetof(Vertex, tangent)));

	// Bitangent
	glEnableVertexAttribArray(4);
	glVertexAttribPointer(4, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
		reinterpret_cast<void*>(offsetof(Vertex, biTangent)));

	glBindVertexArray(0);
}
