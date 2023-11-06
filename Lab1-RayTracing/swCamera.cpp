#include "swCamera.h"

namespace sw {

void Camera::setup(int w, int h) {
    imageWidth = w;
    imageHeight = h;

    forward = lookAt - origin;
    forward.normalize();
    right = forward % up;
    right.normalize();
    up = right % forward;

    imageExtentX = std::tan(0.5f * vFOV * static_cast<float>(M_PI) / 180.0f);
    imageExtentY = std::tan(0.5f * vFOV / aspectRatio * static_cast<float>(M_PI) / 180.0f);
}

Ray Camera::getRay(float x, float y) {
    Vec3 xIncr = 2.0f / ((float)imageWidth) * imageExtentX * right;
    Vec3 yIncr = -2.0f / ((float)imageHeight) * imageExtentY * up;
    Vec3 view = forward - imageExtentX * right + imageExtentY * up;

    return Ray(origin, view + x * xIncr + y * yIncr, 0.0f, FLT_MAX);
}

} // namespace sw
