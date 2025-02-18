#version 430 core

layout (binding = 0, std430) readonly buffer velocitiesBuffer {
    vec4 velocities[];
};

layout (binding = 1, std430) readonly buffer positionsBuffer {
    vec4 positions[];
};

layout (binding = 2, std430) buffer accBuffer {
    vec4 accelerations[];
};

layout (std140) uniform Matrices {
    mat4 projection;
    mat4 view;
    vec4 camPos;
};

out vec3 VertexCol;

const float minPointSize = 13.0f; 
const float pointScale = 95.0f;

float maxVel = 16; 

vec3 BRcolormap(vec3 vel) {
    float velocityMagnitude = length(vel);
    float normalizedLen = clamp(velocityMagnitude / maxVel, 0.0, 1.0);

    vec3 colSlow = vec3(0.2, 0.2, 1.0);
    vec3 colMid = vec3(1.0, 0.5, 0.0);
    vec3 colFast = vec3(1.0, 1.0, 0.0); 

    float smoothVel1 = smoothstep(0.0, 0.5, normalizedLen);
    float smoothVel2 = smoothstep(0.5, 1.0, normalizedLen);

    vec3 color = mix(colSlow, colMid, smoothVel1);
    color = mix(color, colFast, smoothVel2);

    return color; 
}

vec3 RWcolormap(vec3 vel) {
    float len = length(vel);
    float normalizedLen = clamp(len / maxVel, 0, 1);
    float lerped = mix(0.2, 3.0, normalizedLen);
    return vec3(
        lerped <= 1.0 ? lerped : 1.0,
        lerped <= 2.0 ? lerped - 1.0 : 1.0,
        lerped <= 3.0 ? lerped - 2.0 : 1.0
    );
}

float getPointSize(vec3 pointPos) {
    // scale size to distance from the camera
    float size = pointScale / length(camPos.xyz - pointPos);
    // for debugging
    // if (size > 1.0) {
    //     VertexCol = vec3(1.0,1.0,0);
    // }
    return size;
}

void main() {
    vec3 pos = positions[gl_VertexID].xyz;
    vec3 vel = velocities[gl_VertexID].xyz;

    VertexCol = BRcolormap(vel);

    gl_Position = projection * view * vec4(pos.xyz, 1.0);
    gl_PointSize = max(getPointSize(pos), minPointSize); 
}