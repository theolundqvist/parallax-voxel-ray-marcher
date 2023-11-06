#pragma once

#include "swMaterial.h"

namespace sw {

class Primitive {
  public:
    virtual ~Primitive() {}

    virtual bool intersect(const Ray &r, Intersection &isect) const = 0;

  public:
    Material material;
};

} // namespace sw
