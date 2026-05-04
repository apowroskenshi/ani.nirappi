#version 430 core
#define PI 3.14159265359

layout(location = 0) out vec4 FragVisibility;
layout(location = 1) out vec4 FragRadiance;
in vec2 TexCoords;

uniform sampler2D gPosition;
uniform sampler2D gNormal;
uniform sampler2D gAlbedo;
uniform sampler2D envMap;

uniform mat4 view;
uniform mat4 projection;
uniform vec3 viewPos;

uniform float R;
uniform int n;
uniform float contrast;
uniform float lod;

struct Light {
    vec3 position;
    vec3 color;    
    float intensity;
    bool enable;
};
uniform Light light;



float hash(vec2 p) {
    return fract(sin(dot(p, vec2(12.9898, 78.233))) * 43758.5453);
}

vec2 uvOfW(vec3 w) {
    w = normalize(w);
    return vec2(
        0.5 - (atan(w.x, w.z) / (2.0 * PI)), 
        acos(w.y) / PI 
    );
}

vec3 hemisphereDir(int index, int total, float phi, vec3 N) {
    vec3 up = abs(N.y) < 0.999 ? vec3(0.0, 1.0, 0.0) : vec3(1.0, 0.0, 0.0);
    vec3 T = normalize(cross(up, N));
    vec3 B = cross(N, T);

    float alpha = (float(index) + 0.5) / float(total);
    float theta = 2.0 * PI * alpha * (7.0 * float(total) / 9.0) + phi;

    float cosElevation = 1.0 - alpha;
    float sinElevation = sqrt(1.0 - cosElevation * cosElevation);

    vec3 localDir = vec3(
        sinElevation * cos(theta),
        sinElevation * sin(theta),
        cosElevation
    );

    return normalize(T * localDir.x + B * localDir.y + N * localDir.z);
}

void main() {
    vec3 P = texture(gPosition, TexCoords).xyz;
    vec3 N = texture(gNormal, TexCoords).xyz;    

    if (N == vec3(0.0)) {
        FragVisibility = vec4(1.0, 1.0, 1.0, 1.0);
        FragRadiance = vec4(0.0, 0.0, 0.0, 1.0);
        return;
    }

    vec3 Ldir = vec3(0.0);
    float deltaOmega = 2.0 * PI / float(n);
    float phi = PI * fract(52.9829189 * fract(0.06711056 * gl_FragCoord.x + 0.00583715 * gl_FragCoord.y));

    float maxLod = float(textureQueryLevels(envMap) - 1);    
    ivec2 envSize = textureSize(envMap, 0);
    //float lod = clamp(0.5 * log2(float(envSize.x * envSize.y) / (2.0 * float(n))), 0.0, maxLod);    

    int occluderCount = 0;

    float totalLight = 0.0;
    float visibleLight = 0.0;

    // Hemisphere sampling loop
    for (int i = 0; i < n; i++) {
        vec3 sampleDir = hemisphereDir(i, n, phi, N);          
        float lambda = R * hash(gl_FragCoord.xy + float(i)); // Select random length in the hemisphereDir
        vec3 samplePoint = P + sampleDir * lambda;        

        // Project to screen
        vec4 clipPos = projection * view * vec4(samplePoint, 1.0);
        clipPos.xy /= clipPos.w;
        vec2 sampleUV = clipPos.xy * 0.5 + 0.5;
        
        float cosTheta = max(0.0, dot(N, sampleDir));
        vec3 Lin = textureLod(envMap, uvOfW(sampleDir), lod).rgb;
        float weight = dot(Lin, vec3(0.2126, 0.7152, 0.0722)) * cosTheta;

        totalLight += weight;
        
        bool visible = false;

        if (sampleUV.x < 0.0 || sampleUV.x > 1.0 || sampleUV.y < 0.0 || sampleUV.y > 1.0) {
            visible = true;
        } else {
            vec3 surfacePos = texture(gPosition, sampleUV).xyz;
            vec3 surfaceNormal = texture(gNormal, sampleUV).xyz;

            if (surfaceNormal == vec3(0.0)) {
                visible = true;
            } else {
                float sampleDist = length(samplePoint - viewPos);
                float surfaceDist = length(surfacePos - viewPos);
                visible = (sampleDist <= surfaceDist);
            }
        }

        if (visible) {
            visibleLight += weight;
            Ldir += (1.0 / PI) * Lin * cosTheta * deltaOmega;
        } else {
            occluderCount++;
        }
    }

    
    float visibility = totalLight > 0.0 ? visibleLight / totalLight : 1.0;            
    visibility = clamp(pow(visibility, contrast), 0.0, 1.0);
    
    FragVisibility = vec4(vec3(visibility), 1.0);    
    FragRadiance = vec4(Ldir, 1.0);
}