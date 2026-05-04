#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoords;
layout (location = 3) in vec3 aTangent;
layout (location = 4) in vec3 aBiTangent;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
uniform mat4 sceneScale;

out vec4 position;
out vec4 worldPos;

void main()
{        
    gl_Position = projection * view * model * vec4(aPos, 1.0);;    
    worldPos = model * vec4(aPos, 1.0);    
    position = gl_Position;
}