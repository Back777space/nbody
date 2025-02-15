#version 430 core
out vec4 FragColor;

in vec3 VertexCol;

void main() {
    FragColor = vec4(VertexCol, 1.0);
}
