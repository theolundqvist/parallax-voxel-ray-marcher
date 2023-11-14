#include "swTriangle.h"
#include "swVec3.h"
#include <cmath>

namespace sw {

float length(Vec3 v) { return sqrt(v * v); }

bool Triangle::intersect(const Ray &r, Intersection &isect) const {
    // TODO: Implement ray-triangle intersection

    Vec3 o = r.orig;
    Vec3 d = r.dir;

    Vec3 v0 = vertices[0];
    Vec3 e1 = vertices[1] - v0;
    Vec3 e2 = vertices[2] - v0;
    Vec3 n = (e1 % e2);
    // n * (o + t*d - V0) = 0
    // n * o + n * t * d - n * V0 = 0
    // t = (-n * o + n * V0)/nd
    float t = (-n * o + n * v0) / (n * d);
    if (t >= r.maxT || t <= r.minT) return false;

    Vec3 Q = o + t * d;

    Vec3 coplanar = Q - v0;

    // first/second
    if ((e1 % coplanar) * n < 0) return false;
    if ((coplanar % e2) * n < 0) return false;

    // third barycentric
    float v = length(e1 % coplanar) / length(n);
    float w = length(coplanar % e2) / length(n);
    if (v + w >= 1) return false;

    isect.material = material;
    isect.ray = r;
    isect.position = o + t * d;
    isect.frontFacing = n * d > 0;
    isect.normal = n.normalize();
    if (isect.frontFacing) isect.normal *= -1;
    isect.hitT = t;

    return true;
}

} // namespace sw
