#version 330 core

layout (location = 0) out vec4 gPosition;
layout (location = 1) out vec4 gNormal;
layout (location = 2) out vec4 gAlbedo;
layout (location = 3) out vec4 gMetalRough;

in VS_OUT {
    vec3 FragPos;
    vec2 TexCoords;
    mat3 TBN;
    float viewZ;
} fs_in;

// PBR Material textures
uniform sampler2D texture_diffuse;
uniform sampler2D texture_normal;
uniform sampler2D texture_roughness;
uniform sampler2D texture_metallic;
uniform sampler2D texture_ao;
uniform sampler2D texture_opacity;

uniform bool disableNormalMaps;

// Material struct
struct Material {
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
    float opacity;
    float metallic;
    float roughness;

    bool hasDiffuseMap;
    bool hasNormalMap;
    bool hasRoughnessMap;
    bool hasMetallicMap;
};

uniform Material material;

void main()
{
    // ── Opacity / alpha test ──
    float opacity;
    if (material.hasDiffuseMap) {
        opacity = texture(texture_diffuse, fs_in.TexCoords).a;
    } else {
        opacity = material.opacity;
    }

    if (opacity < 0.5)
        discard;

    // ── Position (World Space) ──
    gPosition = vec4(fs_in.FragPos, fs_in.viewZ);

    // ── Normal (World Space) with normal mapping ──
    vec3 N;
    if (material.hasNormalMap && !disableNormalMaps) {
        vec3 normalSample = texture(texture_normal, fs_in.TexCoords).rgb;
        vec3 normalTS = normalize(normalSample * 2.0 - 1.0);
        N = normalize(fs_in.TBN * normalTS);
    } else {
        // No normal map — use vertex normal (third column of TBN)
        N = normalize(fs_in.TBN[2]);
    }

    // After computing N (normal), before writing to gNormal:
    if (!gl_FrontFacing) {
        N = -N;
    }

    gNormal = vec4(N, 1.0);

    // ── Albedo (base color) ──
    vec3 albedo;
    if (material.hasDiffuseMap) {
        albedo = texture(texture_diffuse, fs_in.TexCoords).rgb;
    } else {
        albedo = material.diffuse;
    }
    gAlbedo = vec4(albedo, 1.0);

    // ── Metallic ──
    float metallic;
    if (material.hasMetallicMap) {
        metallic = texture(texture_metallic, fs_in.TexCoords).r;
    } else {
        metallic = material.metallic;
    }

    // ── Roughness ──
    float roughness;
    if (material.hasRoughnessMap) {
        roughness = texture(texture_roughness, fs_in.TexCoords).r;
    } else {
        roughness = material.roughness;
    }

    gMetalRough = vec4(metallic, roughness, 0.0, 0.0);
}
