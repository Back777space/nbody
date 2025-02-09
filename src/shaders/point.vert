#version 430 core

layout (location = 0) in vec4 aPos;
layout (location = 1) in vec4 aVel;

layout (std140) uniform Matrices
{
    mat4 projection;
    mat4 view;
};

out vec3 VertexCol;

void main()
{
    float len = length(aVel.xyz); 
    vec3 unitVec = normalize(aVel.xyz); 
    vec3 color = 0.5 + 0.5 * unitVec;  
    float minBrightness = 0.55; 
    // interpolate
    VertexCol = mix(vec3(minBrightness), color, len); 

    gl_Position = projection * view * vec4(aPos.xyz, 1.0);
    gl_PointSize = 2; 
}