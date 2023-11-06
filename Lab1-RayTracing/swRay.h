#pragma once

#include "swVec3.h"

namespace sw {

class Ray {
  public:
    Ray() = default;
    Ray(const Vec3 &o, const Vec3 &d, float t0 = 0.0f, float t1 = FLT_MAX) : orig(o), dir(d), minT(t0), maxT(t1) {}

    Vec3 origin() const { return orig; }
    Vec3 direction() const { return dir; }

  public:
    Vec3 orig;
    Vec3 dir;
    float minT{0.0f};
    float maxT{FLT_MAX};
};

} // namespace sw
