#pragma once
#include "vec3.hpp"

struct hit_record {
    point3 p;
    vec3 normal;
    double t{};
};