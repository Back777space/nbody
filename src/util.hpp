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

std::ostream& operator<<(std::ostream& s, glm::vec2 vec) {
    s << "(" << vec.x << ", " << vec.y << ")";
    return s;
}

template<class T>
std::ostream& operator<<(std::ostream& out, const std::vector<T>& obj) {
    out << "[";
    for (size_t i = 0; i < obj.size(); i++) {
        out << obj[i];
        if (i != obj.size() - 1) out << ", ";
    }
    return  out << "]";
}