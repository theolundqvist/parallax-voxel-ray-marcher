#pragma once

#include "Direction.cpp"
#include <glm/ext/matrix_transform.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

class Transform {
public:
    glm::mat4 mRT = glm::mat4(1.0f);
    glm::mat4 mS = glm::mat4(1.0f);

    void setPos(glm::vec3 pos) {
        mRT[3][0] = pos.x;
        mRT[3][1] = pos.y;
        mRT[3][2] = pos.z;
    }

    void translate(glm::vec3 translation) {
        mRT = glm::translate(glm::mat4(1.0f), translation) * mRT;
    }

    glm::vec3 getPos() { return {mRT[3][0], mRT[3][1], mRT[3][2]}; }

    void scale(glm::vec3 scale) { mS = glm::scale(glm::mat4(1.0f), scale) * mS; }

    void scale(float s) { scale(glm::vec3(s)); }

    void setScale(glm::vec3 s) {
        mS = glm::mat4(1.0f);
        scale(s);
    }

    void setScale(float s) {
        mS = glm::mat4(1.0f);
        scale(s);
    }

    glm::vec3 getScale() { return {mS[0][0], mS[1][1], mS[2][2]}; }

    glm::mat4 getMatrix() const { return mRT * mS; }

    void setMatrix(glm::mat4 mat) {
        mRT = mat;
    }

    void rotateAroundX(float angle) {
        mRT = glm::rotate(glm::mat4(1.0f), angle, glm::vec3(1, 0, 0)) * mRT;
    }

    void rotateAroundY(float angle) {
        mRT = glm::rotate(glm::mat4(1.0f), angle, glm::vec3(0, 1, 0)) * mRT;
    }

    void rotateAroundZ(float angle) {
        mRT = glm::rotate(glm::mat4(1.0f), angle, glm::vec3(0, 0, 1)) * mRT;
    }

    void resetRotation() {
        auto translation = getPos();
        mRT = glm::mat4x4(1.0f);
        setPos(translation);
    }

    glm::vec3 getEulerAngles() const { return glm::eulerAngles(glm::quat_cast(mRT)); }

    glm::vec3 getUp() const { return applyRotation(Direction::up); }

    glm::vec3 getFront() const { return applyRotation(Direction::front); }

    glm::vec3 getRight() const { return applyRotation(Direction::right); }

    glm::vec3 apply(glm::vec3 vec) const { return {mRT * glm::vec4(vec, 1)}; }

    glm::vec3 applyRotation(glm::vec3 vec) const {
        return {mRT * glm::vec4(vec, 0)};
    }

    void lookAt(glm::vec3 at, glm::vec3 up) { lookTowards(at - getPos(), up); }

    void lookTowards(glm::vec3 front_vec, glm::vec3 up_vec) {
        front_vec = normalize(-front_vec);
        up_vec = normalize(up_vec);

        if (std::abs(dot(up_vec, front_vec)) > 0.99999f)
            return;

        glm::vec3 prev_up = up_vec;

        glm::vec3 right = cross(front_vec, prev_up);
        glm::vec3 up = cross(right, front_vec);

        right = normalize(right);
        up = normalize(up);

        mRT[0] = glm::vec4(right, 0);
        mRT[1] = glm::vec4(up, 0);
        mRT[2] = glm::vec4(-front_vec, 0);
    }

    void printMatrix() {
        printf("Matrix:\n");
        printf("%.4f %.4f %.4f %.4f\n", mRT[0][0], mRT[1][0], mRT[2][0], mRT[3][0]);
        printf("%.4f %.4f %.4f %.4f\n", mRT[0][1], mRT[1][1], mRT[2][1], mRT[3][1]);
        printf("%.4f %.4f %.4f %.4f\n", mRT[0][2], mRT[1][2], mRT[2][2], mRT[3][2]);
        printf("%.4f %.4f %.4f %.4f\n", mRT[0][3], mRT[1][3], mRT[2][3], mRT[3][3]);
    }
};
