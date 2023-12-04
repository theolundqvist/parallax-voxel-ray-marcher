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

    // found this algorithm here
    // https://tavianator.com/2022/ray_box_boundary.html
    static bool RayIntersectsBox(glm::vec3 min, glm::vec3 max, glm::vec3 origin, glm::vec3 dir_inv) {
        double tx1 = (min.x - origin.x) * dir_inv.x;
        double tx2 = (min.x - origin.x) * dir_inv.x;

        double tmin = std::min(tx1, tx2);
        double tmax = std::max(tx1, tx2);

        double ty1 = (min.y - origin.y) * dir_inv.y;
        double ty2 = (max.y - origin.y) * dir_inv.y;

        tmin = std::max(tmin, std::min(ty1, ty2));
        tmax = std::min(tmax, std::max(ty1, ty2));

        return tmax >= tmin;
    }

};