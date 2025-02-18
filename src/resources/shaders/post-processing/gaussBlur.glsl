#version 430 core

layout (local_size_x = 16, local_size_y = 16, local_size_z = 1) in;

layout(binding = 0, rgba16f) uniform image2D inputImage;
layout(binding = 1, rgba16f) uniform image2D outputImage;

const int kernel_size = 5;
const float weight[5] = float[](0.227027, 0.1945946, 0.1216216, 0.054054, 0.016216); // gaussian weights

uniform bool horizontal;

void main() {
    ivec2 coord = ivec2(gl_GlobalInvocationID.xy);
    ivec2 imgSize = imageSize(inputImage);

    if (all(lessThan(coord, imgSize))) {
        vec3 result =  imageLoad(inputImage, coord).rgb * weight[0]; // middle texel
        ivec2 dir = horizontal ? ivec2(1, 0) : ivec2(0, 1);

        for (int i = 1; i < kernel_size; i++) {
            const ivec2 offset = dir * i;
            const ivec2 pos1 = coord + offset;
            const ivec2 pos2 = coord - offset;

            bvec2 valid1 = lessThan(pos1, imgSize);
            bvec2 valid2 = greaterThanEqual(pos2, ivec2(0));

            // if the offset goes out of bounds the last factor will be zero
            // this way we dont need a conditional check and avoid branch divergence
            result += imageLoad(inputImage, pos1).rgb * weight[i] * float(horizontal ? valid1.x : valid1.y);
            result += imageLoad(inputImage, pos2).rgb * weight[i] * float(horizontal ? valid2.x : valid2.y);
        }

        imageStore(outputImage, coord, vec4(result, 1.0));
    }

}
