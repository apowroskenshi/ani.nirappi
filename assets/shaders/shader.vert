#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoords;
layout (location = 3) in vec3 aTangent;
layout (location = 4) in vec3 aBiTangent; // Assuming biTangent is location 4 in your Mesh class setup

out VS_OUT {
    vec3 FragPos;
    vec2 TexCoords;
    mat3 TBN; // TBN matrix transforms from Tangent Space -> World Space
} vs_out;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

void main()
{
    gl_Position = projection * view * model * vec4(aPos, 1.0);
    
    vs_out.FragPos = vec3(model * vec4(aPos, 1.0));

    // Calculate TBN matrix in World Space
    mat3 normalMatrix = transpose(inverse(mat3(model)));
    
    vec3 N = normalize(normalMatrix * aNormal);
    vec3 T = normalize(normalMatrix * aTangent);
    vec3 B = normalize(normalMatrix * aBiTangent); // Use the provided bitangent

    // Re-orthogonalize T and N (Gram-Schmidt process for safety)
    T = normalize(T - dot(T, N) * N);

    // Construct the TBN matrix (columns T, B, N)
    vs_out.TBN = mat3(T, B, N);

    vs_out.TexCoords = aTexCoords;
}
