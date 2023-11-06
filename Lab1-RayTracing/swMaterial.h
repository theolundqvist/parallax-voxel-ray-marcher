#pragma once

#include "swVec3.h"

namespace sw {

class Material {
  public:
    Material() = default;
    // color, reflectivity, transparency, refractiveIndex
    Material(const Vec3 &c, float r = 0, float t = 0, float i = 1)
      : color{c}, reflectivity(r), transparency(t), refractiveIndex(i) {}

  public:
    Vec3 color;
    float reflectivity{0.0f};
    float transparency{0.0f};
    float refractiveIndex{1.0f};
};

} // namespace sw
