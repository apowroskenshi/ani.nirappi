#version 330 core
#define PI 3.14159265359
out vec4 FragColor;

in vec3 TexCoords;

uniform sampler2D equirectangularMap;
uniform float exposure;
uniform bool enableACES;

const vec2 invAtan = vec2(0.1591, 0.3183); // 1/(2*PI), 1/PI

// ACES Filmic Tone Mapping (Approximate)
vec3 aces(vec3 x) {
    float a = 2.51;
    float b = 0.03;
    float c = 2.43;
    float d = 0.59;
    float e = 0.14;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}


vec2 SampleSphericalMap(vec3 v) {
    
    vec3 n = normalize(v);

    // 1. Horizontal: Must match IBL's atan(n.x, n.z)
    float u = fract(0.5 - (atan(n.x, n.z) / (2.0 * PI)));

    // 2. Vertical: Must match IBL's acos(n.y)        
    float v_tex = acos(n.y) / PI;

    return vec2(u, v_tex);
}

void main() {
    // Calculate UVs from the 3D direction
    vec2 uv = SampleSphericalMap(normalize(TexCoords));
    vec3 color = texture(equirectangularMap, uv).rgb;    

    // Simple Saturation Boost (apply before tone mapping)
    float luma = dot(color, vec3(0.2126, 0.7152, 0.0722));
    color = mix(vec3(luma), color, 1.1); // 1.1 is a 10% boost

    // 2. Apply Exposure     
    color *= exposure;

    // 3. Tone Mapping (Reinhard) - Prevents "Glow" from blowing out to pure white
    color = aces(color);    
    
    // 4. Gamma Correction (Linear -> sRGB)
    color = pow(color, vec3(1.0/2.2)); 

    FragColor = vec4(color, 1.0);
}