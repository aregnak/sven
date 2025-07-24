#version 330 core

in vec3 Normal;
in vec3 FragPos;
in vec2 TexCoord;
in vec3 Color;

out vec4 FragColor;

uniform vec3 viewPos;

void main() {
    // Simple lighting
    vec3 lightPos = vec3(0.0, 10.0, 0.0);
    vec3 lightColor = vec3(1.0, 1.0, 1.0);
    
    // Ambient
    float ambientStrength = 0.3;
    vec3 ambient = ambientStrength * lightColor;
    
    // Diffuse
    vec3 norm = normalize(Normal);
    vec3 lightDir = normalize(lightPos - FragPos);
    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = diff * lightColor;
    
    // Specular (not really needed for grass)
    float specularStrength = 0.0;
    
    vec3 result = (ambient + diffuse) * Color;
    FragColor = vec4(result, 1.0);
    
    // Add some transparency to edges
    float alpha = smoothstep(0.0, 0.4, TexCoord.y);
    FragColor.a = alpha;
}