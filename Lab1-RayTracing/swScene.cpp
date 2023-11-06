#include "swScene.h"

namespace sw {

bool Scene::intersect(const Ray &r, Intersection &isect, bool any) {
    bool hit = false;
    Intersection currIsect;
    for (auto &primitive : primitives) {
        if (primitive->intersect(r, currIsect)) {
            hit = true;
            if (any) return hit;
            if (currIsect.hitT < isect.hitT) {
                isect = currIsect;
            }
        }
    }
    return hit;
}

} // namespace sw
