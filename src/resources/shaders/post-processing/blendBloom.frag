#version 430 core

out vec4 FragColor;
in vec2 TexCoords;

layout(binding = 0) uniform sampler2D sceneTexture; 
layout(binding = 1) uniform sampler2D bloomTexture; 

const float bloomIntensity = 1.50;

void main() {
    vec3 sceneColor = texture(sceneTexture, TexCoords).rgb;
    vec3 bloomColor = texture(bloomTexture, TexCoords).rgb;

    vec3 finalColor = sceneColor + bloomColor * bloomIntensity; 
    FragColor = vec4(finalColor, 1.0);
}
