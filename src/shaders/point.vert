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
    float minLen = 0.1;   // expected minimum length
    float maxLen = 12.0;  // expected maximum length
    float len = length(aVel.xyz);

    float normalizedLen = clamp((len - minLen) / (maxLen - minLen), 0.0, 1.0);

    float col = mix(120.0/255.0, 3.0, normalizedLen);

    VertexCol = vec3(
        col <= 1.0 ? col : 1.0,
        col <= 2.0 ? col - 1.0 : 1.0,
        col <= 3.0 ? col - 2.0 : 1.0
    );


    gl_Position = projection * view * vec4(aPos.xyz, 1.0);
    gl_PointSize = 1.25; 
}