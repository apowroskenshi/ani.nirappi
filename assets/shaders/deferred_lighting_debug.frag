#version 330 core

out vec4 FragColor;

in vec2 TexCoords;

// G-Buffer inputs
uniform sampler2D shadowMap;
uniform sampler2D blurredShadowMap;
uniform sampler2D gPosition;
uniform sampler2D gNormal;
uniform sampler2D gAlbedo;
uniform sampler2D gMetalRough;
uniform sampler2D ssaoMap;
uniform sampler2D blurredssaoMap;

uniform sampler2D rawSSDOVisibilityMap;
uniform sampler2D blurSSDOVisibilityMap;
uniform sampler2D rawSSDORadianceMap;
uniform sampler2D blurSSDORadianceMap;
uniform sampler2D contactShadowMap;

uniform sampler2D indirectSSDOMap;
uniform sampler2D blurindirectSSDOMap;
uniform sampler2D ssrMap;

uniform mat4 shadowMatrix;
uniform float sceneSize;

uniform int debugMode;
uniform bool useOrthographicShadows;
uniform float nearPlane;
uniform float farPlane;


// Point lights
struct PointLight {
    vec3 position;
    vec3 color;
    float radius;
    float intensity;
    float farPlane;
    float nearPlane;
};

uniform PointLight pointLight;

void main()
{
    // Visualize Position
    if (debugMode == 0) {         
        vec3 pos = texture(gPosition, TexCoords).rgb;
        if (pos == vec3(0.0)) {            
            FragColor = vec4(0.0, 0.0, 0.0, 1.0);
        } else {
            //vec3 normalized = (pos * 0.5 + 0.5);             
            //FragColor = vec4(clamp(normalized, 0.0, 1.0), 1.0);
            vec3 normalized = 0.5 + 0.5 * sign(pos) * log(abs(pos) + 1.0) / log(3.0);
            FragColor = vec4(normalized, 1.0);
        }
    }
    // Visualize Normal (normals are -1 to 1, map to 0-1)
    else if (debugMode == 1) {        
        vec3 normal = texture(gNormal, TexCoords).rgb;
        if (normal == vec3(0.0)) {            
            FragColor = vec4(0.0, 0.0, 0.0, 1.0);
        } else {
            FragColor = vec4(normal * 0.5 + 0.5, 1.0);
            //FragColor = vec4(abs(normal), 1.0);
        }
    }
    else if (debugMode == 2) {        
        float depth = -texture(gPosition, TexCoords).w;  // negate because view space Z is negative
        float normalized = (depth - 0.1) / (7.0 - 0.1);
        FragColor = vec4(vec3(normalized), 1.0);
    }
    // Visualize Albedo
    else if (debugMode == 3) {        
        vec3 albedo = texture(gAlbedo, TexCoords).rgb;
        FragColor = vec4(albedo, 1.0);
    } 
    // Visualize Metallic
    else if (debugMode == 4) {        
        float metallic = texture(gMetalRough, TexCoords).r;
        FragColor = vec4(vec3(metallic), 1.0);
    } 
    // Visualize Roughness
    else if (debugMode == 5) {        
        float roughness = texture(gMetalRough, TexCoords).g;
        FragColor = vec4(vec3(roughness), 1.0);
    }
    // Visualize Shadow Map
    else if (debugMode == 6 || debugMode == 7) {      
    
        float d = texture(shadowMap, TexCoords).r;
        FragColor = vec4(texture(shadowMap, TexCoords).rgb, 1.0);
        return;

       vec3 pos = texture(gPosition, TexCoords).rgb;
        
        // Transform position to shadow space
        vec4 shadowCoord = shadowMatrix * vec4(pos, 1.0);
        vec2 shadowTexCoord = shadowCoord.xy / shadowCoord.w;

        // Black out pixels with no geometry (skybox, background)
        if (pos == vec3(0.0)) {
            FragColor = vec4(0.0, 0.0, 0.0, 1.0);
            return;
        }                
        
        // Clamp to valid range [0,1]
        if (shadowTexCoord.x < 0.0 || shadowTexCoord.x > 1.0 || 
            shadowTexCoord.y < 0.0 || shadowTexCoord.y > 1.0) {
            FragColor = vec4(0.0, 0.0, 0.0, 1.0);
        } else {            
            float pixelDepth, lightDepth, visualDepth;
            
            // Orthographic: use .z
            pixelDepth = shadowCoord.z;  

            if(debugMode == 6) {
                lightDepth = texture(shadowMap, shadowTexCoord).r;                
                FragColor = vec4(texture(shadowMap, shadowTexCoord).rgb, 1.0f);
                return;    

            } else {
                lightDepth = texture(blurredShadowMap, shadowTexCoord).r;     
                FragColor = vec4(lightDepth, pow(lightDepth, 2), pow(lightDepth, 3), pow(lightDepth,4));
                return;    
            }                                               
        }                
                       
    }    
    else if (debugMode == 9) {
        float ssao = texture(ssaoMap, TexCoords).r;
        FragColor = vec4(vec3(ssao), 1.0);
    }
    else if (debugMode == 10) {
        float ssao = texture(blurredssaoMap, TexCoords).r;
        FragColor = vec4(vec3(ssao), 1.0);
    }
    else if (debugMode == 11) {
        vec3 ssdo = texture(rawSSDOVisibilityMap, TexCoords).rgb;
        FragColor = vec4(ssdo, 1.0);
    }
    else if (debugMode == 12) {
        vec3 blurssdo = texture(blurSSDOVisibilityMap, TexCoords).rgb;
        FragColor = vec4(blurssdo, 1.0);
    }
    else if (debugMode == 13) {
        vec3 ssdo = texture(rawSSDORadianceMap, TexCoords).rgb;
        FragColor = vec4(ssdo, 1.0);
    }
    else if (debugMode == 14) {
        vec3 blurssdo = texture(blurSSDORadianceMap, TexCoords).rgb;
        FragColor = vec4(blurssdo, 1.0);
    }
    else if (debugMode == 15) {
        vec3 issdo = texture(indirectSSDOMap, TexCoords).rgb;
        FragColor = vec4(issdo, 1.0);
    }
    else if (debugMode == 16) {
        vec3 blurissdo = texture(blurindirectSSDOMap, TexCoords).rgb;
        FragColor = vec4(blurissdo, 1.0);
    }
    // SSR Color (what the reflection looks like)
    else if (debugMode == 17) {
        vec4 ssr = texture(ssrMap, TexCoords);
        FragColor = vec4(ssr.rgb, 1.0);
    }
    // SSR Confidence (where rays hit — white = hit, black = miss)
    else if (debugMode == 18) {
        float confidence = texture(ssrMap, TexCoords).a;
        FragColor = vec4(vec3(confidence), 1.0);
    }
    // SSR Color * Confidence (final SSR contribution)
    else if (debugMode == 19) {
        vec4 ssr = texture(ssrMap, TexCoords);
        FragColor = vec4(ssr.rgb * ssr.a, 1.0);
    }
}