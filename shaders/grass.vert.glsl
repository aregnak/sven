#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoord;
layout (location = 3) in vec3 instancePos;
layout (location = 4) in float instanceWidth;
layout (location = 5) in float instanceHeight;
layout (location = 6) in vec3 instanceColor;
layout (location = 7) in float instanceRotation;

out vec3 Normal;
out vec3 FragPos;
out vec2 TexCoord;
out vec3 Color;

uniform mat4 view;
uniform mat4 projection;
uniform float time;
uniform vec3 windDirection;
uniform float windStrength;

mat4 rotationMatrix(vec3 axis, float angle) {
    axis = normalize(axis);
    float s = sin(angle);
    float c = cos(angle);
    float oc = 1.0 - c;
    
    return mat4(oc * axis.x * axis.x + c,           oc * axis.x * axis.y - axis.z * s,  oc * axis.z * axis.x + axis.y * s,  0.0,
                oc * axis.x * axis.y + axis.z * s,  oc * axis.y * axis.y + c,           oc * axis.y * axis.z - axis.x * s,  0.0,
                oc * axis.z * axis.x - axis.y * s,  oc * axis.y * axis.z + axis.x * s,  oc * axis.z * axis.z + c,           0.0,
                0.0,                                0.0,                                0.0,                                1.0);
}

mat4 translationMatrix(vec3 translation) {
    return mat4(
        1.0, 0.0, 0.0, 0.0,
        0.0, 1.0, 0.0, 0.0,
        0.0, 0.0, 1.0, 0.0,
        translation.x, translation.y, translation.z, 1.0
    );
}

mat4 scaleMatrix(vec3 scale) {
    return mat4(
        scale.x, 0.0, 0.0, 0.0,
        0.0, scale.y, 0.0, 0.0,
        0.0, 0.0, scale.z, 0.0,
        0.0, 0.0, 0.0, 1.0
    );
}

void main() {
    // Apply wind animation
    float windEffect = sin(time * 2.0 + instancePos.x * 10.0) * 0.2 * windStrength;
    mat4 windRotation = rotationMatrix(vec3(0.0, 0.0, 1.0), windEffect * dot(windDirection, vec3(1.0, 0.0, 0.0)));
    
    // Create transformations
    mat4 rotate = rotationMatrix(vec3(0.0, 1.0, 0.0), radians(instanceRotation));
    mat4 scale = scaleMatrix(vec3(instanceWidth, instanceHeight, instanceWidth));
    mat4 translate = translationMatrix(instancePos);
    
    // Combine transformations
    mat4 modelMatrix = mat4(1.0);
    modelMatrix = translate * modelMatrix;
    modelMatrix = modelMatrix * rotate;
    modelMatrix = modelMatrix * windRotation;
    modelMatrix = modelMatrix * scale;
    
    gl_Position = projection * view * modelMatrix * vec4(aPos, 1.0);
    
    FragPos = vec3(modelMatrix * vec4(aPos, 1.0));
    Normal = mat3(transpose(inverse(modelMatrix))) * aNormal;
    TexCoord = aTexCoord;
    Color = instanceColor;
}