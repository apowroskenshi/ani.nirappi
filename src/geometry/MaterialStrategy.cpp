#include "geometry/MaterialStrategy.h"

#include <cmath>
#include <algorithm>

#include <glm/glm.hpp>

#include "core/Logger.h"

// --- Factory ---

std::unique_ptr<MaterialStrategy> createMaterialStrategy(ModelFormat format)
{
	switch (format) {
		case ModelFormat::GLTF:
			return std::make_unique<PbrMaterialStrategy>();
		case ModelFormat::FBX:
			// FBX can contain PBR data in modern exports — use PBR strategy.
			// If this proves wrong for specific FBX files, add a dedicated FbxMaterialStrategy.
			return std::make_unique<PbrMaterialStrategy>();
		case ModelFormat::OBJ:
		case ModelFormat::Unknown:
		default:
			return std::make_unique<PhongMaterialStrategy>();
	}
}

// ============================================================================
// PBR Material Strategy (glTF, modern FBX)
// ============================================================================

Material PbrMaterialStrategy::extractMaterial(const aiMaterial* material) const
{
	Material mtl;

	// Base color
	aiColor4D baseColor(1.0f, 1.0f, 1.0f, 1.0f);
	material->Get(AI_MATKEY_BASE_COLOR, baseColor);
	mtl.diffuse = glm::vec3(baseColor.r, baseColor.g, baseColor.b);
	mtl.ambient = mtl.diffuse * 0.1f;

	// Metallic and roughness
	float metallic = 0.0f;
	float roughness = 0.5f;
	material->Get(AI_MATKEY_METALLIC_FACTOR, metallic);
	material->Get(AI_MATKEY_ROUGHNESS_FACTOR, roughness);

	mtl.metallic = metallic;
	mtl.roughness = glm::clamp(roughness, 0.05f, 1.0f);

	// Compute dielectric F0 from IOR
	float ior = 1.5f;
	material->Get(AI_MATKEY_REFRACTI, ior);
	float f0 = pow((ior - 1.0f) / (ior + 1.0f), 2.0f);
	mtl.specular = glm::vec3(f0);

	// Opacity
	float opacity = 1.0f;
	material->Get(AI_MATKEY_OPACITY, opacity);
	mtl.opacity = opacity;

	return mtl;
}

std::vector<TextureMapping> PbrMaterialStrategy::getTextureMappings() const
{
	return {
		// Diffuse: prefer BASE_COLOR (glTF), fall back to DIFFUSE (legacy)
		{
			"texture_diffuse",
			{ aiTextureType_BASE_COLOR, aiTextureType_DIFFUSE },
			false, {}
		},
		// Normal: prefer NORMALS (glTF/standard), fall back to HEIGHT (OBJ convention)
		{
			"texture_normal",
			{ aiTextureType_NORMALS, aiTextureType_HEIGHT },
			true, glm::vec4(0.5f, 0.5f, 1.0f, 1.0f)
		},
		// Roughness
		{
			"texture_roughness",
			{ aiTextureType_DIFFUSE_ROUGHNESS },
			true, glm::vec4(0.5f, 0.5f, 0.5f, 1.0f)
		},
		// Metallic
		{
			"texture_metallic",
			{ aiTextureType_METALNESS },
			true, glm::vec4(0.0f, 0.0f, 0.0f, 1.0f)
		},
	};
}

// ============================================================================
// Phong Material Strategy (OBJ/MTL)
// ============================================================================

Material PhongMaterialStrategy::extractMaterial(const aiMaterial* material) const
{
	Material mtl;

	aiColor3D diffuseColor(0.0f, 0.0f, 0.0f);
	material->Get(AI_MATKEY_COLOR_DIFFUSE, diffuseColor);
	bool hasDiffuseColor = (diffuseColor.r + diffuseColor.g + diffuseColor.b) > 0.01f;

	aiColor3D specularColor(0.0f, 0.0f, 0.0f);
	material->Get(AI_MATKEY_COLOR_SPECULAR, specularColor);

	aiColor3D ambientColor(0.0f, 0.0f, 0.0f);
	material->Get(AI_MATKEY_COLOR_AMBIENT, ambientColor);

	// Detect Assimp's placeholder material (all ones = no real .mtl data)
	bool isDefault = (diffuseColor.r == 1.0f && diffuseColor.g == 1.0f && diffuseColor.b == 1.0f
		&& specularColor.r == 1.0f && specularColor.g == 1.0f && specularColor.b == 1.0f);

	if (isDefault) {
		mtl.ambient = glm::vec3(0.5f);
		mtl.diffuse = glm::vec3(0.5f);
		mtl.specular = glm::vec3(0.04f);
	}
	else {
		mtl.ambient = glm::vec3(ambientColor.r, ambientColor.g, ambientColor.b);
		mtl.diffuse = hasDiffuseColor
			? glm::vec3(diffuseColor.r, diffuseColor.g, diffuseColor.b)
			: glm::vec3(0.5f);

		glm::vec3 Ks = (specularColor.r + specularColor.g + specularColor.b > 0.001f)
			? glm::vec3(specularColor.r, specularColor.g, specularColor.b)
			: glm::vec3(0.04f);

		if (glm::length(mtl.diffuse - Ks) < 0.01f) {
			Ks = glm::vec3(0.04f);
		}
		mtl.specular = Ks;
	}

	// Shininess (Ns) → PBR roughness
	float Ns = 0.0f;
	if (material->Get(AI_MATKEY_SHININESS, Ns) != AI_SUCCESS || Ns <= 0.0f) {
		Ns = 50.0f;
	}
	mtl.roughness = 1.0f - sqrt(glm::clamp(Ns / 1000.0f, 0.0f, 1.0f));

	// Modulate roughness by specular intensity
	float ksIntensity = (specularColor.r + specularColor.g + specularColor.b) / 3.0f;
	mtl.roughness = glm::clamp(mtl.roughness * (1.0f - ksIntensity * 0.5f), 0.05f, 1.0f);

	mtl.metallic = 0.0f;

	// Opacity
	float opacity = 1.0f;
	material->Get(AI_MATKEY_OPACITY, opacity);
	mtl.opacity = opacity;

	return mtl;
}

std::vector<TextureMapping> PhongMaterialStrategy::getTextureMappings() const
{
	return {
		// Diffuse: OBJ uses DIFFUSE, but also try BASE_COLOR for modern .mtl
		{
			"texture_diffuse",
			{ aiTextureType_DIFFUSE, aiTextureType_BASE_COLOR },
			false, {}
		},
		// Normal: OBJ convention is HEIGHT, but also try NORMALS
		{
			"texture_normal",
			{ aiTextureType_HEIGHT, aiTextureType_NORMALS },
			true, glm::vec4(0.5f, 0.5f, 1.0f, 1.0f)
		},
		// Roughness
		{
			"texture_roughness",
			{ aiTextureType_DIFFUSE_ROUGHNESS },
			true, glm::vec4(0.5f, 0.5f, 0.5f, 1.0f)
		},
		// Metallic
		{
			"texture_metallic",
			{ aiTextureType_METALNESS },
			true, glm::vec4(0.0f, 0.0f, 0.0f, 1.0f)
		},
	};
}
