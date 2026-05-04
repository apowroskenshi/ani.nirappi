#pragma once

#include <string>
#include <vector>
#include <memory>
#include <filesystem>
#include <unordered_map>
#include <unordered_set>

#include <glm/glm.hpp>

#include <assimp/scene.h>
#include <assimp/matrix4x4.h>

#include "geometry/Mesh.h"
#include "material/Texture.h"
#include "geometry/MaterialStrategy.h"

struct LoadedModel {
	std::vector<Mesh> meshes;
	glm::vec3 sceneMin{FLT_MAX};
	glm::vec3 sceneMax{-FLT_MAX};
};

class ModelLoader {
public:
	LoadedModel load(const std::string& modelFileName);

private:
	static inline const std::filesystem::path kModelDir{"assets/model"};

	static ModelFormat detectFormat(const std::string& filePath);

	// Node traversal with accumulated transforms
	void processNode(const aiNode* node, const aiScene* scene,
		const aiMatrix4x4& parentTransform);

	// Mesh extraction — format-agnostic, driven by strategy
	Mesh processMesh(const aiMesh* mesh, const aiScene* scene,
		const aiMatrix4x4& transform);

	// Texture loading — tries types in priority order from TextureMapping
	void loadTextures(const aiMaterial* material, const aiScene* scene,
		const std::vector<TextureMapping>& mappings,
		std::vector<std::shared_ptr<Texture>>& outTextures,
		Material& outMaterial);

	void loadMaterialTextures(const aiMaterial* material, aiTextureType texType,
		const std::string& typeName, const aiScene* scene,
		std::vector<std::shared_ptr<Texture>>& outTextures);

	// Path utilities
	static std::filesystem::path getModelDirectory(const std::string& modelFileName);
	static std::string getModelFilePath(const std::string& modelFileName);

	// State for current load
	std::unique_ptr<MaterialStrategy> m_strategy;
	std::filesystem::path m_directory;
	std::unordered_map<std::string, std::shared_ptr<Texture>> m_textureCache;
	std::unordered_set<unsigned int> m_processedMeshIndices;

	// Output accumulated during loading
	std::vector<Mesh> m_meshes;
	glm::vec3 m_sceneMin{FLT_MAX};
	glm::vec3 m_sceneMax{-FLT_MAX};
};
