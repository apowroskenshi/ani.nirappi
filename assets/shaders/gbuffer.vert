#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoords;
layout (location = 3) in vec3 aTangent;
layout (location = 4) in vec3 aBiTangent;

out VS_OUT {
    vec3 FragPos;
    vec2 TexCoords;
    mat3 TBN;
    float viewZ;
} vs_out;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;


void main()
{
    vec4 worldPos = model * vec4(aPos, 1.0);
    vec4 viewPos = view * worldPos;

    gl_Position = projection * viewPos;
    vs_out.FragPos = worldPos.xyz;
    vs_out.viewZ = viewPos.z;

    // Calculate TBN matrix in World Space
    mat3 normalMatrix = transpose(inverse(mat3(model)));
    
    vec3 N = normalize(normalMatrix * aNormal);
    vec3 T = normalize(normalMatrix * aTangent);
     
     // If bitangent is provided (non-zero), use it; otherwise compute it
    vec3 B;
    if (length(aBiTangent) > 0.1) {
        B = normalize(normalMatrix * aBiTangent);
    } else {
        B = cross(N, T);
    }
        
    // Orthogonalize T with respect to N
    T = normalize(T - dot(T, N) * N);
    
    // Recompute B from orthogonal N and T to ensure perfect orthogonality
    B = cross(N, T);
    B = normalize(B);


    vs_out.TBN = mat3(T, B, N);
    vs_out.TexCoords = aTexCoords;
}