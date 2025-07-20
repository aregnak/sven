#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec2 aTexCoord;
layout (location = 2) in vec3 aNormal;

// Instanced matrix attributes
layout (location = 3) in vec4 modelMat0;
layout (location = 4) in vec4 modelMat1;
layout (location = 5) in vec4 modelMat2;
layout (location = 6) in vec4 modelMat3;

out vec2 TexCoord;

uniform mat4 view;
uniform mat4 projection;

void main()
{
    // Construct model matrix from instanced attributes
    mat4 model = mat4(modelMat0, modelMat1, modelMat2, modelMat3);
    
    gl_Position = projection * view * model * vec4(aPos, 1.0);
    TexCoord = aTexCoord;
} 