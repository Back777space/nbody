#version 430 core
out vec4 FragColor;

in vec3 VertexCol;
in vec2 ObjSpace;
in float pixelSize; 

const float radius = 0.1;

void main() {
    if (dot(ObjSpace, ObjSpace) > radius * radius) 
        discard;

    float alpha = 1.0 - smoothstep(1.0 - 3.0 * pixelSize, 1.0, length(ObjSpace));
    FragColor = vec4(VertexCol, 1.0);
}
