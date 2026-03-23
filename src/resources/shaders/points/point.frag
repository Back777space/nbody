#version 430 core

in vec3 VertexCol;
in float AccMagnitude;

out vec4 FragColor;

const float radiusSq = 0.25;

void main() {
    vec2 distToCenter = gl_PointCoord - vec2(0.5);
    if (dot(distToCenter, distToCenter) > radiusSq) {
        discard;
    }

    // acceleration adds brightness
    vec3 color = VertexCol * (0.8 + 0.4 * AccMagnitude);

    FragColor = vec4(color, 1.0);
}
