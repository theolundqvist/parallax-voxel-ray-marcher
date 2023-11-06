#include "swSphere.h"

namespace sw {

bool solveQuadratic(float A, float B, float C, float &t0, float &t1) {
    float d = B * B - 4.0f * A * C;
    if (d < 0.0f) return false;
    d = std::sqrt(d);
    t0 = (-B - d) / (2.0f * A);
    t1 = (-B + d) / (2.0f * A);
    return true;
}

bool Sphere::intersect(const Ray &r, Intersection &isect) const {
    Vec3 o = r.orig - center;
    Vec3 d = r.dir;

    // Compute polynom coefficients.
    float A = d * d;
    float B = 2.0f * d * o;
    float C = o * o - radius * radius;

    // Solve quadratic equation for ray enter/exit point t0, t1 respectively.
    float t0, t1;
    if (!solveQuadratic(A, B, C, t0, t1)) return false; // no real solutions -> ray missed

    if (t0 > r.maxT || t1 < r.minT) {
        return false; // sphere before/after ray
    }
    if (t0 < r.minT && t1 > r.maxT) return false; // ray inside sphere

    isect.hitT = t0 < r.minT ? t1 : t0;
    isect.normal = (o + (isect.hitT) * d) * (1.0f / radius);
    isect.normal.normalize();
    isect.frontFacing = (-d * isect.normal) > 0.0f;
    if (!isect.frontFacing) isect.normal = -isect.normal;
    isect.position = o + (isect.hitT) * d + center;
    isect.material = material;
    isect.ray = r;
    return true;
}

} // namespace sw
