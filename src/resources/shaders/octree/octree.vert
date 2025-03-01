#version 430 core

layout (location = 0) in vec3 aPos;

layout (std140) uniform Matrices {
    mat4 projection;
    mat4 view;
    vec4 camPos;
};

void main() {
    gl_Position = projection * view * vec4(aPos, 1.0);
}