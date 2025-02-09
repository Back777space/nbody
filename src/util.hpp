#pragma once
#include <stdlib.h>
#include <memory>

template<typename T> 
using P = std::unique_ptr<T>;

float randFloat(float max) {
    return static_cast <float> (rand()) / (static_cast <float> (RAND_MAX/max));
}

std::ostream& operator<<(std::ostream& s, glm::vec3 vec) {
    s << "(" << vec.x << ", " << vec.y << ", " << vec.z << ")";
    return s;
}