#pragma once

#include "Direction.cpp"
#include <glm/ext/matrix_transform.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

class Transform {
public:
    glm::mat4 mRT = glm::mat4(1.0f);
    glm::mat4 mS = glm::mat4(1.0f);

    Transform setPos(glm::vec3 pos) {
        mRT[3][0] = pos.x;
        mRT[3][1] = pos.y;
        mRT[3][2] = pos.z;
        return *this;
    }

    Transform translate(glm::vec3 translation) {
        mRT = glm::translate(glm::mat4(1.0f), translation) * mRT;
        return *this;
    }
    Transform translate(float x, float y, float z) { return translate(glm::vec3(x, y, z)); }
    Transform translateX(float x) { return translate(glm::vec3(x, 0, 0)); }
    Transform translateY(float y) { return translate(glm::vec3(0, y, 0)); }
    Transform translateZ(float z) { return translate(glm::vec3(0, 0, z)); }


    glm::vec3 getPos() { return {mRT[3][0], mRT[3][1], mRT[3][2]}; }

    Transform scale(glm::vec3 scale) {
        mS = glm::scale(glm::mat4(1.0f), scale) * mS;
        return *this;
    }

    Transform scale(float s) { return scale(glm::vec3(s)); }

    Transform setScale(glm::vec3 s) {
        mS = glm::mat4(1.0f);
        return scale(s);
    }

    Transform setScale(float s) {
        mS = glm::mat4(1.0f);
        return scale(s);
    }

    glm::vec3 getScale() { return {mS[0][0], mS[1][1], mS[2][2]}; }

    glm::mat4 getMatrix() const { return mRT * mS; }

    Transform setMatrix(glm::mat4 mat) {
        mRT = mat;
        return *this;
    }

    Transform rotateAroundX(float angle) {
        mRT = glm::rotate(glm::mat4(1.0f), angle, glm::vec3(1, 0, 0)) * mRT;
        return *this;
    }

    Transform rotateAroundY(float angle) {
        mRT = glm::rotate(glm::mat4(1.0f), angle, glm::vec3(0, 1, 0)) * mRT;
        return *this;
    }

    Transform rotateAroundZ(float angle) {
        mRT = glm::rotate(glm::mat4(1.0f), angle, glm::vec3(0, 0, 1)) * mRT;
        return *this;
    }

    Transform resetRotation() {
        auto translation = getPos();
        mRT = glm::mat4x4(1.0f);
        return setPos(translation);
    }

    glm::vec3 getEulerAngles() const { return glm::eulerAngles(glm::quat_cast(mRT)); }

    glm::vec3 getUp() const { return applyRotation(Direction::up); }

    glm::vec3 getFront() const { return applyRotation(Direction::front); }

    glm::vec3 getRight() const { return applyRotation(Direction::right); }

    glm::vec3 apply(glm::vec3 vec) const { return {getMatrix() * glm::vec4(vec, 1)}; }

    glm::vec3 applyRotation(glm::vec3 vec) const {
        return {mRT * glm::vec4(vec, 0)};
    }

    Transform get_inverse() const{
        return Transform().setMatrix(glm::inverse(getMatrix()));
    }

    Transform lookAt(glm::vec3 at, glm::vec3 up) { return lookTowards(at - getPos(), up); }

    Transform lookTowards(glm::vec3 front_vec, glm::vec3 up_vec) {
        front_vec = normalize(-front_vec);
        up_vec = normalize(up_vec);

        if (std::abs(dot(up_vec, front_vec)) > 0.99999f)
            return *this;

        glm::vec3 prev_up = up_vec;

        glm::vec3 right = cross(front_vec, prev_up);
        glm::vec3 up = cross(right, front_vec);

        right = normalize(right);
        up = normalize(up);

        mRT[0] = glm::vec4(right, 0);
        mRT[1] = glm::vec4(up, 0);
        mRT[2] = glm::vec4(-front_vec, 0);
        return *this;
    }

    void printMatrix() {
        printf("Matrix:\n");
        printf("%.4f %.4f %.4f %.4f\n", mRT[0][0], mRT[1][0], mRT[2][0], mRT[3][0]);
        printf("%.4f %.4f %.4f %.4f\n", mRT[0][1], mRT[1][1], mRT[2][1], mRT[3][1]);
        printf("%.4f %.4f %.4f %.4f\n", mRT[0][2], mRT[1][2], mRT[2][2], mRT[3][2]);
        printf("%.4f %.4f %.4f %.4f\n", mRT[0][3], mRT[1][3], mRT[2][3], mRT[3][3]);
    }
};
