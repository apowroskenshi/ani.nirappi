#version 430 core
#define PI 3.14159265359

out vec4 FragColor;
in vec2 TexCoords;

uniform sampler2D gPosition;
uniform sampler2D gNormal;
uniform sampler2D ssaoMap;

uniform float gaussianWeights[31];
uniform int radius;
uniform float s;


void main() {

	float d = texture(gPosition, TexCoords).w;	
	vec3 N = texture(gNormal, TexCoords).xyz;
	vec2 texelSize = 1.0 / vec2(textureSize(ssaoMap, 0));	

	float totalWeight = 0.0f, result = 0.0f;


	for(int x = -radius; x <= radius; x++) {
		for(int y = -radius; y <= radius; y++) {
		
			vec2 sampleCords = TexCoords + vec2(x, y) * texelSize;
			
			float aoSample = texture(ssaoMap, sampleCords).r;			
			vec3 Ni = texture(gNormal, sampleCords).xyz;
			float di = texture(gPosition, sampleCords).w;

			float spatial = gaussianWeights[abs(x)] * gaussianWeights[abs(y)];

			float normalWeight = max(0.0f, dot(N, Ni));
			float depthDiff = (d - di);
			float depthWeight = exp(-(depthDiff * depthDiff)/ (2.0 * s));

			float W = spatial * normalWeight * depthWeight;

			result += aoSample * W;
			totalWeight += W;
		
		}	
	}

	FragColor = vec4(vec3(result / totalWeight), 1.0);

}