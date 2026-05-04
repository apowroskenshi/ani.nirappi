#version 430 core
#define PI 3.14159265359

out vec4 FragColor;

in vec2 TexCoords;

// G-Buffer inputs
uniform sampler2D shadowMap;
uniform sampler2D blurredShadowMap;

uniform sampler2D gPosition;
uniform sampler2D gNormal;
uniform sampler2D gAlbedo;
uniform sampler2D gMetalRough;
uniform sampler2D irrMap, irrMapv1;
uniform sampler2D envMap;
uniform sampler2D ssaoMap;
uniform sampler2D blurredssaoMap;

uniform sampler2D blurSSDOVisibilityMap;
uniform sampler2D blurSSDORadianceMap;
uniform sampler2D blurindirectSSDOMap;
uniform sampler2D contactShadowMap;
uniform sampler2D ssrMap;

uniform vec3 viewPos;
uniform float exposure = 1.0;
uniform mat4 shadowMatrix;
uniform mat4 lightViewMatrix;

struct MomentShadow { 
    float scale;
    float visScale;
    float bias;
    float bleedReduction;
    bool useDirectScaledMoments;
    bool enable;
    float normalBias;
    float depthBias;
};
uniform MomentShadow momentShadow;

struct Light {
    vec3 position;
    vec3 color;    
    float intensity;
    bool enable;
};
uniform Light light;


struct IBL {
    int N;
    bool enable;
    bool diffuseOnly;
    float intensity;
};
uniform IBL ibl;

struct SSAO {
    bool enable;
    bool enableBlur;
};
uniform SSAO ssao;

struct SSDO {
  bool enableIndirect;
  bool enableContactShadows;
  bool enable;
  int doMethod;
};

uniform SSDO ssdo;

struct SSR {
    bool enable;
};
uniform SSR ssr;

layout(std140, binding = 0) uniform HammersleyBlock {
    vec4 samples[2058]; 
};

uniform vec3 shCoeffs[9];

vec3 evalSHIrradiance(vec3 n) {
    float x = n.x, y = n.y, z = n.z;  // no remapping

    float Y[9];
    Y[0] = 0.5  * sqrt(1.0  / PI);
    Y[1] = 0.5  * sqrt(3.0  / PI) * y;
    Y[2] = 0.5  * sqrt(3.0  / PI) * z;
    Y[3] = 0.5  * sqrt(3.0  / PI) * x;
    Y[4] = 0.5  * sqrt(15.0 / PI) * x * y;
    Y[5] = 0.5  * sqrt(15.0 / PI) * y * z;
    Y[6] = 0.25 * sqrt(5.0  / PI) * (3.0 * z*z - 1.0);
    Y[7] = 0.5  * sqrt(15.0 / PI) * x * z;
    Y[8] = 0.25 * sqrt(15.0 / PI) * (x*x - y*y);

    vec3 irradiance = vec3(0.0);
    for (int k = 0; k < 9; k++)
        irradiance += shCoeffs[k] * Y[k];

    return max(irradiance, vec3(0.0));
}


// Normal Distribution Function (Trowbridge-Reitz GGX)
float DistributionGGX(vec3 N, vec3 H, float roughness)
{
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;

    float num = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;

    return num / denom;
}

// Geometry Function (Smith's Method with Schlick-GGX G1)
float GeometrySchlickGGX(float NdotV, float roughness)
{    
    
        float k = (roughness * roughness) / 2.0; 

        float num = NdotV;
        float denom = NdotV * (1.0 - k) + k;

        return num / denom;   
}

float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness)
{
    float NdotV = max(dot(N, V), 0.001);
    float NdotL = max(dot(N, L), 0.001);
    float ggx1 = GeometrySchlickGGX(NdotV, roughness);
    float ggx2 = GeometrySchlickGGX(NdotL, roughness);

    return ggx1 * ggx2;
}

// Fresnel Function (Schlick approximation)
vec3 FresnelSchlick(float cosTheta, vec3 F0)
{
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}


vec2 uvOfW(vec3 w) {
    w = normalize(w);
    return vec2(
        0.5 - (atan(w.x, w.z) / (2.0 * PI)), 
        acos(w.y) / PI 
    );
}


vec3 vectorOf(float u, float v) {
    float phi = (u - 0.5) * 2.0 * PI;
    float theta = v * PI;

    float x = sin(theta) * cos(phi);
    float z = sin(theta) * sin(phi);
    float y = cos(theta);

    
    return vec3(x, y, z);
}

// Importance-sample the GGX NDF to produce a half-vector H in world space.
// The caller derives L = reflect(-V, H).
vec3 sampleGGXHalfVector(float e1, float e2, float roughness, vec3 N) {

    float a = roughness * roughness; // must match DistributionGGX alpha
    float theta = atan(a * sqrt(e2) / sqrt(1.0 - e2));
    vec3 localH = vectorOf(e1, theta / PI);

    // Build tangent frame around N
    vec3 upVec = abs(N.y) < 0.999 ? vec3(0,1,0) : vec3(1,0,0);
    vec3 tangent   = normalize(cross(upVec, N));
    vec3 bitangent = normalize(cross(N, tangent));

    return normalize(tangent * localH.x + N * localH.y + bitangent * localH.z);
}


vec3 computeLightIn(vec3 L, float D, bool useMIS, float roughness) {

    vec2 uv = uvOfW(L);    
    float level = 0.0;        

    float maxLod = float(textureQueryLevels(envMap) - 1);

    ivec2 size = textureSize(envMap, 0);
    float saTexel = (4.0 * PI) / float(size.x * size.y);
    float saSample = 1.0 / (float(ibl.N) * max(D, 0.0001));
    level = 0.5 * log2(saSample / saTexel);
    level = clamp(level, 0.0, maxLod);

    return textureLod(envMap, uv, level).rgb;

}

vec3 CalculatePBRLightIBL(vec3 N, vec3 V, vec3 albedo, float metallic, float roughness) {

        // Diffuse: SH irradiance
        vec3 irradN = evalSHIrradiance(normalize(N));
        vec3 diffuse = (albedo / PI) * irradN;

        // Energy conservation
        vec3 F0 = mix(vec3(0.04), albedo, metallic);
        vec3 F  = FresnelSchlick(max(dot(N, V), 0.0), F0);
        vec3 kS = F;
        vec3 kD = (1.0 - kS) * (1.0 - metallic);

        // Diffuse term (with SSDO if enabled)
        vec3 diffuseTerm;
        if (ssdo.enable) {
            vec3 Lind = vec3(0.0);
            if (ssdo.enableIndirect) {
                Lind = texture(blurindirectSSDOMap, TexCoords).rgb;
            }

            if (ssdo.doMethod == 0) {
                float visibility = texture(blurSSDOVisibilityMap, TexCoords).r;
                diffuseTerm = kD * ((diffuse * visibility * ibl.intensity) + (albedo * Lind));
            } else {
                vec3 Ldir = texture(blurSSDORadianceMap, TexCoords).rgb;
                diffuseTerm = kD * albedo * (Ldir * ibl.intensity + Lind);
            }
        } else {
            diffuseTerm = kD * diffuse;
        }

        if (ibl.diffuseOnly) {
            return diffuseTerm;
        }

        // Specular: GGX importance sampling of env map
        vec3 specColor = vec3(0.0);
        float NdotV = max(dot(N, V), 0.001);
        int acceptedSamples = 0;

        for (int i = 1; i <= ibl.N; i++) {
            float e1 = samples[i-1].x;
            float e2 = samples[i-1].y;

            vec3 H = sampleGGXHalfVector(e1, e2, roughness, N);
            vec3 L = reflect(-V, H);

            float NdotL = dot(N, L);
            if (NdotL > 0.0) {
                NdotL = max(NdotL, 0.001);
                float NdotH = max(dot(N, H), 0.001);
                float HdotV = max(dot(H, V), 0.001);

                float D = DistributionGGX(N, H, roughness);
                float G = GeometrySmith(N, V, L, roughness);
                vec3 Fs = FresnelSchlick(HdotV, F0);

                vec3 Li = computeLightIn(L, D, true, roughness);

                specColor += Li * G * Fs * HdotV / (NdotV * NdotH);

                acceptedSamples++;
            }
        }

        if (acceptedSamples > 0)
            specColor /= float(acceptedSamples);

        // SSR: replace IBL specular with screen-space reflections where available
        if (ssr.enable) {
            vec4 ssrSample = texture(ssrMap, TexCoords);
            specColor = mix(specColor, ssrSample.rgb, ssrSample.a);
        }

        return diffuseTerm + specColor;
}


float computeMomentShadow(vec4 moments, float zf, vec3 N, vec3 L) {
    
    float bias = 1e-6; 
    float alpha = 0.01f / (pow(10, momentShadow.bias));   
    vec4 b_prime = mix(moments, vec4(0.5), alpha);
    
    if(momentShadow.useDirectScaledMoments) {
        b_prime = moments;
    }

    float b1 = b_prime.x;
    float b2 = b_prime.y;
    float b3 = b_prime.z;
    float b4 = b_prime.w;

    float m11 = 1.0;
    float m12 = b_prime.x;
    float m13 = b_prime.y;
    float m22 = b_prime.y;
    float m23 = b_prime.z;
    float m33 = b_prime.w;

    float z1 = 1.0;
    float z2 = zf;
    float z3 = zf * zf ;

    // Cholesky method 
    float a = sqrt(m11);
    float b = m12 / a;
    float c = m13 / a ;
    float d = sqrt(max(0.0, m22 - (b*b)) + bias);
    float e = (m23 - (b*c)) / d;
    float f = sqrt(max(0.0, m33 - (c*c) - (e*e)) + bias);

    float c1_ = z1 / a ;
    float c2_ = (z2 - (b*c1_))/d;
    float c3_ = (z3 - (c * c1_) - (e * c2_))/ f ;

    float c3 = c3_ / f;
    float c2 = (c2_ - (e*c3_))/ d;
    float c1 = (c1_ - (b*c2) - (c*c3)) / a;

    float D = max(0.0, (c2 * c2) - (4.0 * c3 * c1 ));

    float Z2 = (-c2 + sqrt( D )) / (2 * c3);
    float Z3 = (-c2 - sqrt( D )) / (2 * c3);

    if (Z2 > Z3) { float temp = Z2; Z2 = Z3; Z3 = temp; }

    if ( zf <= Z2 ) {
        return 0.0;
    } else if (zf <= Z3) {

        float num = (zf*Z3) - (b1 * (zf + Z3)) + b2;
        float denom = (Z3 - Z2) * (zf - Z2);
        return (num / denom);      
        
    } else {
        float num = (Z2*Z3) - (b1 * (Z2 + Z3)) + b2;
        float denom = (zf - Z2) * (zf - Z3);
        return 1.0 - (num / denom);
    }
    
}

float computeBias(vec4 shadowCoord, vec3 N, vec3 L) {

    return momentShadow.depthBias * (1.0 - dot(N, L));   
    
}

// PBR calculation for a single light
vec3 CalculatePBRLight(vec3 FragPos, vec3 N, vec3 V, vec3 L, vec3 lightColor, vec3 albedo, float metallic, float roughness)
{
    vec3 H = normalize(V + L);
    float NdotV = max(dot(N, V), 0.0);
    
    // F0 calculation: dielectrics at 0.04, metals use albedo
    vec3 F0 = vec3(0.04);
    F0 = mix(F0, albedo, metallic);
    
    // Cook-Torrance BRDF
    float NDF = DistributionGGX(N, H, roughness);
    float G = GeometrySmith(N, V, L, roughness);
    vec3 F = FresnelSchlick(max(dot(H, V), 0.0), F0);
    
    vec3 numerator = NDF * G * F;
    float denominator = 4.0 * NdotV * max(dot(N, L), 0.0);
    denominator = max(denominator, 0.001);
    vec3 specColor = numerator / denominator;
     
    vec3 kS = F;
    vec3 kD = vec3(1.0) - kS;
    kD *= 1.0 - metallic;

    float NdotL = max(dot(N, L), 0.0);     

    return (kD * albedo / PI + specColor) * lightColor * NdotL;
}

// ACES Filmic Tone Mapping (Approximate)
vec3 aces(vec3 x) {
    float a = 2.51;
    float b = 0.03;
    float c = 2.43;
    float d = 0.59;
    float e = 0.14;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}

void main()
{

    // Sample G-Buffer textures
    vec3 FragPos = texture(gPosition, TexCoords).rgb;
    vec3 N = normalize(texture(gNormal, TexCoords).rgb);
    vec3 albedo = texture(gAlbedo, TexCoords).rgb;

    vec2 metalRough = texture(gMetalRough, TexCoords).rg;
    float metallic = metalRough.r;
    float roughness = metalRough.g;
    

    vec3 Lo = vec3(0.0f);    
    vec3 V = normalize(viewPos - FragPos);        
    vec3 color = vec3(0.0f);
    vec3 LAmbient = vec3(0.0f);
    vec3 LD = vec3(0.0f);
    float visibility = 1.0; 

    
    if (ibl.enable) {
        LAmbient = CalculatePBRLightIBL(N, V, albedo, metallic, roughness);
    }

    // Direct Light Contribution
    if (light.enable) {
        vec3 L = normalize(light.position);

        LD = CalculatePBRLight(FragPos, N, V, L, light.color * light.intensity, albedo, metallic, roughness);

        if (momentShadow.enable) {
            vec3 biasedPos = FragPos + N * 0.05;
            vec4 shadowCoord = shadowMatrix * vec4(biasedPos, 1.0);
            vec3 shadowIndex = shadowCoord.xyz / shadowCoord.w;

            if (shadowIndex.x >= 0.0 && shadowIndex.x <= 1.0 &&
                shadowIndex.y >= 0.0 && shadowIndex.y <= 1.0) {

                float bias = computeBias(shadowCoord, N, L);
                float biasedPixelDepth = shadowIndex.z - bias;

                vec4 moments = texture(blurredShadowMap, shadowIndex.xy);
                moments *= momentShadow.scale;

                visibility = computeMomentShadow(moments, biasedPixelDepth, N, L);
                visibility = (1.0 - visibility);

                float p = momentShadow.bleedReduction;
                visibility = clamp((visibility - p) / (1.0 - p), 0.0, 1.0);
                visibility = pow(visibility, momentShadow.visScale);
            }
        }
    }

    float shadowAmbientFactor = mix(0.3, 1.0, visibility);
    color = LAmbient * shadowAmbientFactor + (LD * visibility);

    if (ssao.enable) {
        float ao = ssao.enableBlur ? texture(blurredssaoMap, TexCoords).r : texture(ssaoMap, TexCoords).r;
        color = color * ao;
    }

    // Tone mapping and gamma correction
    color *= exposure;
    color = aces(color);
    color = pow(color, vec3(1.0 / 2.2));

    FragColor = vec4(color, 1.0);    

}