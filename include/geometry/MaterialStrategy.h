#pragma once

#include <string>
#include <vector>
#include <memory>

#include <glm/glm.hpp>
#include <assimp/material.h>

#include "geometry/Mesh.h"

enum class ModelFormat {
	OBJ,
	GLTF,
	FBX,
	Unknown
};

// Describes how to find a texture type: try Assimp types in priority order,
// create a solid-color fallback if none found.
struct TextureMapping {
	std::string internalType;              // e.g. "texture_diffuse"
	std::vector<aiTextureType> assimpTypes; // try in order until one has textures
	bool required = false;                 // create fallback if not found
	glm::vec4 fallbackColor{0.0f};         // RGBA for the fallback solid texture
};

// Abstract strategy for extracting PBR Material from Assimp material data.
// Each format (OBJ, glTF, FBX, ...) provides its own implementation.
class MaterialStrategy {
public:
	virtual ~MaterialStrategy() = default;

	virtual Material extractMaterial(const aiMaterial* material) const = 0;
	virtual std::vector<TextureMapping> getTextureMappings() const = 0;
};

// glTF PBR metallic-roughness workflow.
// Reads baseColorFactor, metallicFactor, roughnessFactor, IOR.
class PbrMaterialStrategy : public MaterialStrategy {
public:
	Material extractMaterial(const aiMaterial* material) const override;
	std::vector<TextureMapping> getTextureMappings() const override;
};

// OBJ/MTL Phong workflow, converted to PBR.
// Reads Kd, Ks, Ka, Ns, d and maps to metallic-roughness.
class PhongMaterialStrategy : public MaterialStrategy {
public:
	Material extractMaterial(const aiMaterial* material) const override;
	std::vector<TextureMapping> getTextureMappings() const override;
};

// Factory: picks the right strategy for a given format.
std::unique_ptr<MaterialStrategy> createMaterialStrategy(ModelFormat format);
