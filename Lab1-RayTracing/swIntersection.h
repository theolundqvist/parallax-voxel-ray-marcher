#pragma once

#include "swMaterial.h"
#include "swRay.h"

namespace sw {

class Intersection {
  public:
    Ray getShadowRay(const Vec3 &lightPos);
    Ray getReflectedRay(void);
    Ray getRefractedRay(void);

  public:
    Vec3 position;
    Vec3 normal;
    float hitT{FLT_MAX};
    bool frontFacing{true};
    Material material;
    Ray ray; // incoming ray that creates intersection
};

} // namespace sw
