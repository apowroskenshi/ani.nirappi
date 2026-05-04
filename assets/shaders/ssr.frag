#version 430 core

out vec4 FragColor;

in vec2 TexCoords;

uniform sampler2D gPosition;
uniform sampler2D gNormal;
uniform sampler2D gAlbedo;
uniform sampler2D gMetalRough;

uniform mat4 projection;
uniform mat4 view;

uniform vec3 viewPos;
uniform int maxSteps = 64;
uniform float stepSize = 0.1;
uniform float thickness = 0.5;
uniform int binarySearchSteps = 8;

vec3 worldToScreen(vec3 worldPos) {
    vec4 clipPos = projection * view * vec4(worldPos, 1.0);
    if (clipPos.w <= 0.0) return vec3(-1.0);
    vec3 ndc = clipPos.xyz / clipPos.w;
    return vec3(ndc.xy * 0.5 + 0.5, ndc.z);
}

void main() {
    vec3 fragPos = texture(gPosition, TexCoords).rgb;
    vec3 N = normalize(texture(gNormal, TexCoords).rgb);
    vec2 metalRough = texture(gMetalRough, TexCoords).rg;
    float metallic = metalRough.r;
    float roughness = metalRough.g;

    if (roughness > 0.7 || length(fragPos) < 0.001) {
        FragColor = vec4(0.0);
        return;
    }

    vec3 V = normalize(viewPos - fragPos);
    vec3 R = reflect(-V, N);

    // Offset ray origin along normal
    vec3 rayOrigin = fragPos + N * 0.02;

    // Simple world-space ray march
    vec2 hitUV = vec2(0.0);
    bool hit = false;
    float hitDist = 0.0;

    for (int i = 1; i <= maxSteps; i++) {
        float t = stepSize * float(i);
        vec3 rayPoint = rayOrigin + R * t;

        vec3 screenPos = worldToScreen(rayPoint);

        if (screenPos.x < 0.0 || screenPos.x > 1.0 ||
            screenPos.y < 0.0 || screenPos.y > 1.0) {
            break;
        }

        vec4 sampleData = texture(gPosition, screenPos.xy);
        vec3 samplePos = sampleData.rgb;
        float surfaceViewZ = sampleData.a;

        if (length(samplePos) < 0.001) continue;

        // View-space Z comparison (both negative in OpenGL)
        // surfaceViewZ - rayViewZ > 0 means ray went past the surface
        float rayViewZ = (view * vec4(rayPoint, 1.0)).z;
        float depthDiff = surfaceViewZ - rayViewZ;

        // Adaptive thickness: grows with distance to allow for larger steps
        float adaptiveThickness = thickness * (1.0 + t * 0.05);

        if (depthDiff > 0.0 && depthDiff < adaptiveThickness) {
            // Back-face rejection: reflected surface should face toward the ray
            vec3 hitNormal = normalize(texture(gNormal, screenPos.xy).rgb);
            if (dot(hitNormal, R) < 0.0) {
                hitUV = screenPos.xy;
                hitDist = t;
                hit = true;
                break;
            }
            // Back face — keep marching past it
        }
    }

    // Binary search refinement
    if (hit && binarySearchSteps > 0) {
        float tHit = hitDist;
        float tMiss = hitDist - stepSize;

        for (int i = 0; i < binarySearchSteps; i++) {
            float tMid = (tHit + tMiss) * 0.5;
            vec3 midPoint = rayOrigin + R * tMid;
            vec3 screenPos = worldToScreen(midPoint);

            if (screenPos.x < 0.0 || screenPos.x > 1.0 ||
                screenPos.y < 0.0 || screenPos.y > 1.0) {
                tHit = tMid;
                continue;
            }

            vec4 sampleData = texture(gPosition, screenPos.xy);
            if (length(sampleData.rgb) < 0.001) {
                tMiss = tMid;
                continue;
            }

            float rayViewZ = (view * vec4(midPoint, 1.0)).z;
            float depthDiff = sampleData.a - rayViewZ;

            if (depthDiff > 0.0 && depthDiff < thickness) {
                tHit = tMid;
                hitUV = screenPos.xy;
            } else {
                tMiss = tMid;
            }
        }
    }

    if (!hit) {
        FragColor = vec4(0.0);
        return;
    }

    vec3 reflectedColor = texture(gAlbedo, hitUV).rgb;

    // Edge fade
    vec2 edgeFade = smoothstep(0.0, 0.1, hitUV) * (1.0 - smoothstep(0.9, 1.0, hitUV));
    float screenFade = edgeFade.x * edgeFade.y;

    // Roughness fade
    float roughFade = 1.0 - smoothstep(0.0, 0.7, roughness);

    float confidence = screenFade * roughFade;

    FragColor = vec4(reflectedColor, confidence);
}
