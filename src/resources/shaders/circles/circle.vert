#version 430 core

layout (binding = 0, std430) readonly buffer velocitiesBuffer {
    vec4 velocities[];
};

layout (binding = 1, std430) readonly buffer positionsBuffer {
    vec4 positions[];
};

layout (std140) uniform Matrices {
    mat4 projection;
    mat4 view;
};

out vec3 VertexCol;
out vec2 ObjSpace; 
out float pixelSize; 


uniform vec2 viewportSize;

const float sqrt2 = 1.414214;
const float sqrt3 = 1.7321;

const vec2 triangleOffsets[3] = vec2[3](
    vec2(-sqrt3,  -1.0),   // left vertex
    vec2(sqrt3,   -1.0),   // right vertex
    vec2(0.0,      2.0)    // top vertex 
);

const float radius = 0.1;

vec3 BRcolormap(float t) {
    return vec3(
        mix(0.25, 0.85, t),     
        0.35 * (1.0 - t),      
        mix(0.70, 0.0, t)     
    );
}

vec3 RWcolormap(float t) {
    t = mix(0.0, 3.0, t);
    return vec3(
        t <= 1.0 ? t : 1.0,
        t <= 2.0 ? t - 1.0 : 1.0,
        t <= 3.0 ? t - 2.0 : 1.0
    );
}

void main() {
    const int pointID = gl_VertexID / 3;
    const int vertexN = gl_VertexID % 3;
    const vec3 vel = velocities[pointID].xyz;
    const vec3 pos = positions[pointID].xyz;
    
    const float minLen = 0.05; // expected minimum length
    const float maxLen = 13;  // expected maximum length
    float len = dot(vel, vel);

    float normalizedLen = clamp((len - (minLen * minLen)) / ((maxLen * maxLen) - minLen), 0, 1);
    VertexCol = BRcolormap(normalizedLen);

    vec4 basePos = projection * view * vec4(pos, 1.0);
    vec2 offset = triangleOffsets[vertexN];
    vec4 clipPos = vec4(basePos.xy + offset, basePos.zw);
    ObjSpace = offset; 

    vec2 ndcBase = basePos.xy / basePos.w;               
    vec2 ndcOffset = clipPos.xy / clipPos.w;      
    vec2 screenBase = ((ndcBase * 0.5) + 0.5) * viewportSize;
    vec2 screenOffset = ((ndcOffset * 0.5) + 0.5) * viewportSize;

    vec2 deltaPixels = (screenOffset - screenBase) * radius;
    float currentSizeSqrd = dot(deltaPixels, deltaPixels);

    float minSizePixels = 2;

    if (currentSizeSqrd < minSizePixels * minSizePixels) {
        float scaleFactor = minSizePixels / sqrt(currentSizeSqrd);

        vec2 newDelta = deltaPixels * scaleFactor;
        vec2 newScreenPos = screenOffset + newDelta;
        vec2 newNDC = (newScreenPos / viewportSize) * 2.0 - 1.0;

        gl_Position = vec4(newNDC * clipPos.w, clipPos.zw);
        VertexCol = vec3(1.0, 0.0, 0.0);
    } else {
        gl_Position = clipPos;
    }
}