#version 430 core

layout (location = 0) in vec3 aPos;

layout (std140) uniform Matrices
{
    mat4 projection;
    mat4 view;
};

out vec3 VertexCol;

void main()
{
    VertexCol = vec3(1.0, 1.0, 2.0 / aPos.z);

    gl_Position = projection * view * vec4(aPos, 1.0);
    gl_PointSize = 7.0; 
}