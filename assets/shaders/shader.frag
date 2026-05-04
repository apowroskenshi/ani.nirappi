#version 330 core
#define PI 3.14159265359 
out vec4 FragColor;

in VS_OUT {
    vec3 FragPos;
    vec2 TexCoords;
    mat3 TBN;
} fs_in;

// Uniforms matching the typeNames defined in your C++ code
uniform sampler2D texture_diffuse;  // Albedo
uniform sampler2D texture_normal;   // Normal Map
uniform sampler2D texture_specular; // Used as Roughness if roughness map isn't present
uniform sampler2D texture_roughness; // PBR Roughness Map (preferred over specular map)
uniform sampler2D texture_metallic; // PBR Metallic Map

uniform vec3 viewPos;

struct DirLight {
    vec3 direction;
    vec3 color;
};


struct Material {
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
};

uniform DirLight dirLight;
uniform Material material;

// --- PBR Helper Functions ---

// Normal Distribution Function (Trowbridge-Reitz GGX)
float DistributionGGX(vec3 N, vec3 H, float roughness)
{
    float a      = roughness*roughness;
    float a2     = a*a;
    float NdotH  = max(dot(N, H), 0.0);
    float NdotH2 = NdotH*NdotH;

    float nom   = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;

    return nom / denom;
}

// Geometry Function (Smith's Method with Schlick-GGX G1)
float GeometrySchlickGGX(float NdotV, float roughness)
{
    float r = (roughness + 1.0);
    float k = (r*r) / 8.0;

    float nom   = NdotV;
    float denom = NdotV * (1.0 - k) + k;

    return nom / denom;
}

float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness)
{
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx1  = GeometrySchlickGGX(NdotV, roughness);
    float ggx2  = GeometrySchlickGGX(NdotL, roughness);

    return ggx1 * ggx2;
}

// Fresnel Function (Schlick approximation)
vec3 FresnelSchlick(float cosTheta, vec3 F0)
{
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

// ----------------------------


void main()
{

    // --- 1. Get Material Properties from Textures ---
    vec3 albedo = texture(texture_diffuse, fs_in.TexCoords).rgb * material.diffuse;

    float metallic;
    // Check if metallic map is bound, otherwise default to 0.0
    if (textureSize(texture_metallic, 0).x > 1) { metallic = texture(texture_metallic, fs_in.TexCoords).r; }
    else { metallic = 0.0; } 

    float roughness;
    // Check if roughness map is bound, otherwise use specular map as roughness source
    if (textureSize(texture_roughness, 0).x > 1) { roughness = texture(texture_roughness, fs_in.TexCoords).r; }
    else { roughness = texture(texture_specular, fs_in.TexCoords).r; } 


    // --- 2. Normal Mapping (Transform from Tangent Space to World Space) ---
    vec3 N;
    // Check if normal map is bound
    if (textureSize(texture_normal, 0).x > 1) {
        vec3 normalFromMap = texture(texture_normal, fs_in.TexCoords).rgb;
        vec3 normalTS = normalize(normalFromMap * 2.0 - 1.0); 
        N = normalize(fs_in.TBN * normalTS); // Transform to World Space
    } else {
        // Use the interpolated vertex normal (Z-column of the TBN matrix)
        N = normalize(fs_in.TBN[2]); 
    }


    // --- 3. Lighting Vectors (World Space) ---
    vec3 V = normalize(viewPos - fs_in.FragPos);
    vec3 L = normalize(-dirLight.direction); 
    float NdotV = max(dot(N, V), 0.0);

    // --- 4. PBR Calculations (Cook-Torrance) ---
    vec3 F0 = material.specular;
    F0      = mix(F0, albedo, metallic); 

    vec3 Lo = vec3(0.0);
    vec3 H = normalize(V + L); 
    
    float NDF = DistributionGGX(N, H, roughness);   
    float G   = GeometrySmith(N, V, L, roughness);      
    vec3 F    = FresnelSchlick(max(dot(H, V), 0.0), F0);       
    
    vec3 numerator   = NDF * G * F;
    float denominator = 4.0 * NdotV * max(dot(N, L), 0.0);
    denominator = max(denominator, 0.001); 
    vec3 specColor = numerator / denominator;
    
    vec3 kS = F;
    vec3 kD = vec3(1.0) - kS;
    kD *= 1.0 - metallic;

    float NdotL = max(dot(N, L), 0.0);
    vec3 lightColor = dirLight.color;
    Lo += (kD * albedo / PI + specColor) * lightColor * NdotL;


    // --- 5. Ambient and Final Color ---
    vec3 ambient =  material.ambient * albedo; 
    
    vec3 color = ambient + Lo;

    // Tone mapping and gamma correction
    //color = color / (color + vec3(1.0)); 
    //color = pow(color, vec3(1.0/2.2)); 

    FragColor = vec4(color, 1.0);
}
