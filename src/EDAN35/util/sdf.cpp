#pragma once

#include <iostream>
#include <cmath>
#include <vector>
#include <cfloat>
#include "glm/glm.hpp"
#include "../util/noise.cpp"
#include "../util/voxel_util.cpp"
#include "core/helpers.hpp"
#include "core/opengl.hpp"
#include "../util/colorPalette.hpp"

class SDF {
public:
    enum SDFS{
        SPHERE,
        CUBE,
        OCTAHEDRON,
        TORUS,
        LINESEGMENT,
        BOX,
        NBR_SDFS
    };


    GLubyte cube(glm::vec3 size, glm::ivec3 p) {
        std::vector<float> boundaryPoints = {size.x / 2.0f + size.x * 0.3f, size.x / 2.0f - size.x * 0.3f};
        // if the sampler point is inside the cube
        return (p.x < boundaryPoints[0] && p.x > boundaryPoints[1] &&
            p.y < boundaryPoints[0] && p.y > boundaryPoints[1] &&
            p.z < boundaryPoints[0] && p.z > boundaryPoints[1]);
    }

    static bool isInsideSphere(glm::vec3 p, glm::vec3 c, float r) {
        if (glm::length(p - c) < r)
            return true;
        return false;
    }
    // draw a sphere
    static GLubyte sphere(glm::vec3 p, glm::vec3 c, float r) {
        return isInsideSphere(p, c, r);
    }


    // using sphere to generate a cloud


    // draw a octahedron
    static GLubyte octahedron(glm::ivec3 p, glm::ivec3 c, int radius) {
        return (isInsideOctahedron(p, c, radius));
    }

    static bool isInsideOctahedron(glm::ivec3 p, glm::ivec3 c, int radius) {
        std::vector<int> length = {std::abs(p.x - c.x), std::abs(p.y - c.y), std::abs(p.z - c.z)};
        return ((length[0] + length[1] + length[2]) < radius);
    }

    // draw a torus
    static GLubyte torus(glm::vec3 p, glm::vec3 c, glm::vec2 t) {
        return (isInsideTorus(p, c, t));
    }

    static bool isInsideTorus(glm::vec3 p, glm::vec3 c, glm::vec2 t) {
        glm::vec3 c2p = p - c;
        // q.x will less than 0 if the point p is inside a torus
        glm::vec2 q = glm::vec2(std::abs(glm::length(glm::vec2(c2p.x, c2p.z)) - t.x), c2p.y);
        // if (glm::length(q) > t.y) ?
        return (glm::length(q) < t.y);
    }

    // draw a line/capsule
    static GLubyte lineSegment(glm::vec3 start, glm::vec3 end, glm::vec3 p, float r) {
        return (isInsideLineSegment(start, end, p, r));
    }

    static bool isInsideLineSegment(glm::vec3 start, glm::vec3 end, glm::vec3 p, float r) {
        float h = glm::clamp(glm::dot(p - start, end - start) / glm::dot(end - start, end - start), 0.0f, 1.0f);
        float d = glm::length(p - (start - h * (start - end)));
        return (d < r);
    }

    static GLubyte box(glm::vec3 p, glm::vec3 c, glm::vec3 r) {
        return (isInsideBox(p, c, r));
    }

    static bool isInsideBox(glm::vec3 p, glm::vec3 c, glm::vec3 r) {
        glm::vec3 c2p = p - c;
        // don't forget to add abs()
        float d = std::sqrt(std::pow(std::max(std::abs(c2p.x) - r.x, 0.0f), 2.0f) +
                            std::pow(std::max(std::abs(c2p.y) - r.y, 0.0f), 2.0f) +
                            std::pow(std::max(std::abs(c2p.z) - r.z, 0.0f), 2.0f));
        return (d == 0.0f);

    }

    // draw a boxframe
    GLubyte boxFrame(glm::vec3 p, glm::vec3 c, glm::vec3 r, float e) {
        return 0;
    }
};
