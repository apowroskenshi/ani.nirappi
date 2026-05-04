#include "geometry/ModelLoader.h"

#include <algorithm>
#include <cmath>

#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/ProgressHandler.hpp>

#include "core/Logger.h"

namespace {

class ProgressHandler : public Assimp::ProgressHandler {
public:
	bool Update(float percentage) override
	{
		LOG_DEBUG << "Loading Model: " << (percentage * 100.0f) << "%";
		return true;
	}
};

} // namespace

// --- Public API ---

LoadedModel ModelLoader::load(const std::string& modelFileName)
{
	// Reset state
	m_meshes.clear();
	m_textureCache.clear();
	m_processedMeshIndices.clear();
	m_sceneMin = glm::vec3(FLT_MAX);
	m_sceneMax = glm::vec3(-FLT_MAX);

	std::string filePath = getModelFilePath(modelFileName);
	m_directory = getModelDirectory(modelFileName);

	ModelFormat format = detectFormat(filePath);
	m_strategy = createMaterialStrategy(format);

	LOG_DEBUG << "Import Model: " << modelFileName;

	Assimp::Importer importer;
	importer.SetProgressHandler(new ProgressHandler());

	const aiScene* scene = importer.ReadFile(filePath,
		aiProcess_Triangulate
		| aiProcess_CalcTangentSpace
		| aiProcess_GenNormals);

	if (!scene || (scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE) || !scene->mRootNode) {
		LOG_ERROR << "Error importing model: " << importer.GetErrorString();
		return {};
	}

	LOG_DEBUG << "Assimp scene meshes: " << scene->mNumMeshes;

	aiMatrix4x4 identity;
	processNode(scene->mRootNode, scene, identity);
	m_processedMeshIndices.clear();

	LOG_DEBUG << "Total meshes loaded: " << m_meshes.size();

	LoadedModel result;
	result.meshes = std::move(m_meshes);
	result.sceneMin = m_sceneMin;
	result.sceneMax = m_sceneMax;
	return result;
}

// --- Format detection ---

ModelFormat ModelLoader::detectFormat(const std::string& filePath)
{
	std::filesystem::path path(filePath);
	std::string ext = path.extension().string();
	std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

	if (ext == ".gltf" || ext == ".glb") return ModelFormat::GLTF;
	if (ext == ".fbx")                   return ModelFormat::FBX;
	if (ext == ".obj")                   return ModelFormat::OBJ;
	return ModelFormat::Unknown;
}

// --- Node traversal with accumulated transforms ---

void ModelLoader::processNode(const aiNode* node, const aiScene* scene,
	const aiMatrix4x4& parentTransform)
{
	aiMatrix4x4 globalTransform = parentTransform * node->mTransformation;

	for (unsigned int i = 0; i < node->mNumMeshes; i++) {
		unsigned int meshIndex = node->mMeshes[i];

		if (m_processedMeshIndices.contains(meshIndex)) {
			continue;
		}
		m_processedMeshIndices.insert(meshIndex);

		const aiMesh* mesh = scene->mMeshes[meshIndex];

		const aiMaterial* mat = scene->mMaterials[mesh->mMaterialIndex];
		aiString matName;
		mat->Get(AI_MATKEY_NAME, matName);

		std::string name = matName.C_Str();
		if (name.empty() || name == "DefaultMaterial") {
			LOG_DEBUG << "Skipping mesh with no material: " << mesh->mName.C_Str();
			continue;
		}

		m_meshes.push_back(processMesh(mesh, scene, globalTransform));
	}

	for (unsigned int i = 0; i < node->mNumChildren; i++) {
		processNode(node->mChildren[i], scene, globalTransform);
	}
}

// --- Mesh extraction (format-agnostic, driven by strategy) ---

Mesh ModelLoader::processMesh(const aiMesh* mesh, const aiScene* scene,
	const aiMatrix4x4& transform)
{
	std::vector<Vertex> vertices;
	vertices.reserve(mesh->mNumVertices);

	std::vector<unsigned int> indices;
	indices.reserve(mesh->mNumFaces * 3);

	std::vector<std::shared_ptr<Texture>> textures;

	// Normal transform: inverse-transpose of upper 3x3
	aiMatrix3x3 normalMatrix(transform);
	normalMatrix.Inverse().Transpose();

	// --- Vertices with node transform applied ---
	for (unsigned int i = 0; i < mesh->mNumVertices; i++) {
		Vertex vertex{};

		aiVector3D pos = transform * mesh->mVertices[i];
		vertex.position = glm::vec3(pos.x, pos.y, pos.z);

		m_sceneMin = glm::min(m_sceneMin, vertex.position);
		m_sceneMax = glm::max(m_sceneMax, vertex.position);

		if (mesh->HasNormals()) {
			aiVector3D n = normalMatrix * mesh->mNormals[i];
			vertex.normal = glm::normalize(glm::vec3(n.x, n.y, n.z));
		}

		if (mesh->mTextureCoords[0]) {
			vertex.texCoords.x = mesh->mTextureCoords[0][i].x;
			vertex.texCoords.y = mesh->mTextureCoords[0][i].y;

			if (mesh->mTangents) {
				aiVector3D t = normalMatrix * mesh->mTangents[i];
				aiVector3D b = normalMatrix * mesh->mBitangents[i];
				vertex.tangent = glm::normalize(glm::vec3(t.x, t.y, t.z));
				vertex.biTangent = glm::normalize(glm::vec3(b.x, b.y, b.z));
			}
		}

		vertices.emplace_back(vertex);
	}

	// --- Indices ---
	for (unsigned int i = 0; i < mesh->mNumFaces; i++) {
		const aiFace& face = mesh->mFaces[i];
		for (unsigned int j = 0; j < face.mNumIndices; j++) {
			indices.emplace_back(face.mIndices[j]);
		}
	}

	// --- Material and textures (delegated to strategy) ---
	const aiMaterial* material = scene->mMaterials[mesh->mMaterialIndex];
	Material mtl = m_strategy->extractMaterial(material);

	std::vector<TextureMapping> mappings = m_strategy->getTextureMappings();
	loadTextures(material, scene, mappings, textures, mtl);

	return Mesh(std::move(vertices), std::move(indices), std::move(textures), mtl);
}

// --- Texture loading (data-driven by TextureMapping) ---

void ModelLoader::loadTextures(const aiMaterial* material, const aiScene* scene,
	const std::vector<TextureMapping>& mappings,
	std::vector<std::shared_ptr<Texture>>& outTextures,
	Material& outMaterial)
{
	for (const auto& mapping : mappings) {
		int prevSize = static_cast<int>(outTextures.size());

		// Try each Assimp type in priority order
		for (aiTextureType type : mapping.assimpTypes) {
			loadMaterialTextures(material, type, mapping.internalType, scene, outTextures);
			if (static_cast<int>(outTextures.size()) > prevSize) {
				break; // found textures, stop trying fallback types
			}
		}

		bool found = (static_cast<int>(outTextures.size()) > prevSize);

		// Update material flags based on which textures were found
		if (mapping.internalType == "texture_diffuse")
			outMaterial.hasDiffuseMap = found;
		else if (mapping.internalType == "texture_normal")
			outMaterial.hasNormalMap = found;
		else if (mapping.internalType == "texture_roughness")
			outMaterial.hasRoughnessMap = found;
		else if (mapping.internalType == "texture_metallic")
			outMaterial.hasMetallicMap = found;

		// Create fallback solid-color texture if required and not found
		if (!found && mapping.required) {
			const glm::vec4& c = mapping.fallbackColor;
			outTextures.push_back(std::make_shared<Texture>(
				mapping.internalType, c.r, c.g, c.b, c.a));
		}
	}
}

void ModelLoader::loadMaterialTextures(const aiMaterial* material, aiTextureType texType,
	const std::string& typeName, const aiScene* scene,
	std::vector<std::shared_ptr<Texture>>& outTextures)
{
	for (unsigned int i = 0; i < material->GetTextureCount(texType); i++) {
		aiString texPath;
		material->GetTexture(texType, i, &texPath);
		std::string texPathStr = texPath.C_Str();

		auto it = m_textureCache.find(texPathStr);
		if (it != m_textureCache.end()) {
			outTextures.push_back(it->second);
			continue;
		}

		// Embedded textures (glTF/GLB)
		const aiTexture* embeddedTex = scene->GetEmbeddedTexture(texPath.C_Str());
		if (embeddedTex) {
			LOG_WARNING << "Embedded texture not yet supported: " << texPathStr;
			continue;
		}

		auto texture = std::make_shared<Texture>(texPathStr, m_directory, typeName);
		m_textureCache[texPathStr] = texture;
		outTextures.push_back(texture);
	}
}

// --- Path utilities ---

std::filesystem::path ModelLoader::getModelDirectory(const std::string& modelFileName)
{
	return (kModelDir / modelFileName).parent_path();
}

std::string ModelLoader::getModelFilePath(const std::string& modelFileName)
{
	return (kModelDir / modelFileName).string();
}
