//
// Created by Theodor Lundqvist on 2023-12-04.
//
#pragma once

#include <glm/vec3.hpp>
#include <algorithm>

class IntersectionTests {
public:
    // found this algorithm here
    // https://stackoverflow.com/questions/4578967/cube-sphere-intersection-test
    static bool BoxIntersectsSphere(glm::vec3 min, glm::vec3 max, glm::vec3 C, float r) {
        float r2 = r * r;
        float dmin = 0;
        for (int i = 0; i < 3; i++) {
            if (C[i] < min[i]) dmin += (C[i] - min[i]) * (C[i] - min[i]);
            else if (C[i] > max[i]) dmin += (C[i] - max[i]) * (C[i] - max[i]);
        }
        return dmin <= r2;
    }

    typedef struct box_t {
        glm::vec3 min;
        glm::vec3 max;
    } box_t;
    typedef struct ray_t {
        glm::vec3 origin;
        glm::vec3 dir;
        glm::vec3 dir_inv;
    } ray_t;

    typedef struct hit_t {
        bool miss;
        glm::vec3 near;
        glm::vec3 far;
    } hit_t;

    // found this algorithm here
    // https://gdbooks.gitbooks.io/3dcollisions/content/Chapter3/raycast_aabb.html
    static hit_t RayIntersectsBox(box_t box, ray_t ray) {

        float t1 = (box.min.x - ray.origin.x) * ray.dir_inv.x;
        float t2 = (box.max.x - ray.origin.x) * ray.dir_inv.x;
        float t3 = (box.min.y - ray.origin.y) * ray.dir_inv.y;
        float t4 = (box.max.y - ray.origin.y) * ray.dir_inv.y;
        float t5 = (box.min.z - ray.origin.z) * ray.dir_inv.z;
        float t6 = (box.max.z - ray.origin.z) * ray.dir_inv.z;

        float tmin = std::max(std::max(std::min(t1, t2), std::min(t3, t4)), std::min(t5, t6));
        float tmax = std::min(std::min(std::max(t1, t2), std::max(t3, t4)), std::max(t5, t6));

        // if tmax < 0, ray (line) is intersecting AABB, but whole AABB is behing us
        if (tmax < 0) return {true};


        // if tmin > tmax, ray doesn't intersect AABB
        if (tmin > tmax) return {true};


        return {
                .miss = tmax < tmin,
                .near = ray.origin + ray.dir * (float) tmin,
                .far=ray.origin + ray.dir * (float) tmax,
        };
    }

};