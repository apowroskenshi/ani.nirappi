#version 430 core
#define PI 3.14159265359

out vec4 FragColor;
in vec2 TexCoords;

uniform sampler2D gPosition;
uniform sampler2D gNormal;
uniform sampler2D gAlbedo;
uniform sampler2D ssdoRadMap;  // blurred Ldir from direct pass

uniform mat4 view;
uniform mat4 projection;
uniform vec3 viewPos;

uniform float R;
uniform int n;
uniform float dClamp;
uniform float strength;

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
        FragColor = vec4(0.0, 0.0, 0.0, 1.0);
        return;
    }

    vec3 Lind = vec3(0.0);
    float phi = PI * fract(52.9829189 * fract(0.06711056 * gl_FragCoord.x + 0.00583715 * gl_FragCoord.y));
    float As = PI * R * R / float(n);
    int occluderCount = 0;

    for (int i = 0; i < n; i++) {
        vec3 sampleDir = hemisphereDir(i, n, phi, N);
        float lambda = R * hash(gl_FragCoord.xy + float(i)); // Select random length in the hemisphereDir
        vec3 samplePoint = P + sampleDir * lambda;        

        // Project to screen
        vec4 clipPos = projection * view * vec4(samplePoint, 1.0);
        clipPos.xy /= clipPos.w;
        vec2 sampleUV = clipPos.xy * 0.5 + 0.5;

        // Off screen or no geometry — not an occluder, skip
        if (sampleUV.x < 0.0 || sampleUV.x > 1.0 || sampleUV.y < 0.0 || sampleUV.y > 1.0)
            continue;

        vec3 surfacePos = texture(gPosition, sampleUV).xyz;
        vec3 surfaceNormal = texture(gNormal, sampleUV).xyz;
        if (surfaceNormal == vec3(0.0)) continue;

        // Visibility test
        float sampleDist = length(samplePoint - viewPos);
        float surfaceDist = length(surfacePos - viewPos);

        if (sampleDist > surfaceDist) {
            
            occluderCount++;

            // OCCLUDER — bounce its direct light toward P
            vec3 Lpixel = texture(ssdoRadMap, sampleUV).rgb;            
            vec3 senderAlbedo = texture(gAlbedo, sampleUV).rgb;
            Lpixel *= senderAlbedo;
            
            vec3 surfaceToP = P - surfacePos;
            float actualDist = length(surfaceToP);

            if (actualDist < 0.001) continue;

            vec3 dir = surfaceToP / actualDist;
            float d = max(dClamp, actualDist);

            // Sender faces toward P
            float cosTheta_s = max(0.0, dot(surfaceNormal, dir));
            // Receiver faces toward sender
            float cosTheta_r = max(0.0, dot(N, -dir));
            
            float falloff = max(0.0, 1.0 - actualDist / R);

            Lind += (1.0 / PI) * Lpixel * As * cosTheta_s * cosTheta_r * strength * falloff / (d * d);                        
        }
    }

    FragColor = vec4(Lind, 1.0);
}