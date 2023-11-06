#pragma once

#include <vector>

#include "swIntersection.h"
#include "swPrimitive.h"
#include "swRay.h"

namespace sw {

class Triangle : public Primitive {
  public:
    Triangle() = default;
    Triangle(const Vec3 *v, const Material &m) : vertices(v), material(m) {}
    Triangle(const Triangle &t) = default;
    Triangle(Triangle &&) = default;
    Triangle &operator=(Triangle &&) = default;

    bool intersect(const Ray &r, Intersection &isect) const;

  public:
    const Vec3 *vertices;
    Material material;
};

} // namespace sw
