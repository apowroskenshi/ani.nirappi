#version 430 core
in vec2 TexCoords;

uniform sampler2D gPosition;
uniform sampler2D gNormal;
uniform sampler2D ssdoMap;

uniform float gaussianWeights[30];
uniform int radius;
uniform float s;

layout(location = 0) out vec4 BlurredOut0;
layout(location = 1) out vec4 BlurredOut1;

uniform sampler2D inputMap0;  // visibility or indirect
uniform sampler2D inputMap1;  // radiance (only used in MRT mode)
uniform int blurMode;  

void main() {
    float d = texture(gPosition, TexCoords).w;
    vec3 N = texture(gNormal, TexCoords).xyz;
    vec2 texelSize = 1.0 / vec2(textureSize(inputMap0, 0));

    if (N == vec3(0.0)) {
        BlurredOut0 = vec4(1.0, 1.0, 1.0, 1.0);
        BlurredOut1 = vec4(0.0, 0.0, 0.0, 1.0);
        return;
    }

    float totalWeight = 0.0;
    vec3 result0 = vec3(0.0);
    vec3 result1 = vec3(0.0);

    for (int x = -radius; x <= radius; x++) {
        for (int y = -radius; y <= radius; y++) {
            vec2 sampleCoords = TexCoords + vec2(x, y) * texelSize;

            vec3 sample0 = texture(inputMap0, sampleCoords).rgb;
            vec3 Ni = texture(gNormal, sampleCoords).xyz;
            float di = texture(gPosition, sampleCoords).w;

            float spatial = gaussianWeights[abs(x)] * gaussianWeights[abs(y)];
            float normalWeight = max(0.0, dot(N, Ni));
            float depthDiff = (d - di);
            float depthWeight = exp(-(depthDiff * depthDiff) / (2.0 * s));

            float W = spatial * normalWeight * depthWeight;

            result0 += sample0 * W;

            if (blurMode == 1) {
                vec3 sample1 = texture(inputMap1, sampleCoords).rgb;
                result1 += sample1 * W;
            }

            totalWeight += W;
        }
    }

    BlurredOut0 = vec4(result0 / totalWeight, 1.0);
    
    if (blurMode == 1) {
        BlurredOut1 = vec4(result1 / totalWeight, 1.0);
    }
}