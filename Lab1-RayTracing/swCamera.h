#pragma once

#include "swRay.h"

namespace sw {

class Camera {
  public:
    Camera() = default;
    Camera(const Vec3 &o, const Vec3 &at, const Vec3 &u, const float &v, const float &a)
      : origin(o), lookAt(at), up(u), vFOV(v), aspectRatio(a) {}

    void setup(int w, int h);
    Ray getRay(float x, float y);

  public:
    Vec3 origin;
    Vec3 lookAt;
    Vec3 forward, right, up;
    float vFOV{0.0f};
    float aspectRatio{1.0f};
    float imageExtentX{0.0f}, imageExtentY{0.0f};
    int imageWidth{0}, imageHeight{0};
};

} // namespace sw
