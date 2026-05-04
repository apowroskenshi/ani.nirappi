#version 330 core
layout (location = 0) out vec4 FragMoments;

in vec4 position;
in vec4 worldPos;
uniform bool useOrthographicShadows;
uniform float nearPlane;
uniform float farPlane;

void main() {
    float depth = gl_FragCoord.z;
    float depth2 = depth * depth;
    FragMoments = vec4(depth, depth2, depth2 * depth, depth2 * depth2);
       //FragMoments = vec4(worldPos.xyz * 0.05 + 0.5, 1.0);
}