#pragma once
#include <stdlib.h>
#include <memory>
#include "include/glm/gtx/string_cast.hpp"

template<typename T> 
using P = std::unique_ptr<T>;

#define PI 3.14159265359
#define TAU 2*PI

float randFloat(float min = 0.f, float max = 1.f) {
    return min + static_cast <float> (rand()) /( static_cast <float> (RAND_MAX/(max-min)));
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