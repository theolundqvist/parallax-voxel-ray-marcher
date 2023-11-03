#pragma once

#include <glm/glm.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/io.hpp>

#include <iostream>

/**
 * A TRS-transform M is composed of rotation, translation and scale. We define
 * M as:
 *
 *    M = T * R * S
 *
 * Transformations can be used hierarchically. This means that every transformable child
 * node will inherit the transformations of its parent. Let's say for instance,
 * that there are two nodes A and B. A is the parent of B and both are
 * transformable. The local transformation of A is denoted Ma and for B its Ma.
 * The matrix that brings nodes A and B to world space is denoted Wa and Wa.
 * The resulting transformations for the nodes will then be:
 *
 *    Ma = Ta * Ra * Sa
 *    Mb = Tb * Rb * Sb
 *
 *    Wa = Ma = Ta * Ra * Sa
 *    Wb = Ma * Mb = (Ta * Ra * Sa) * (Ta * Rb * Sb)
 *
 * The MB matrix will then, in turn, be recursively be applied to any children
 * of node B to construct new model->world matrices, in the same manner as in
 * the example above.
 *
 */
template<typename T, glm::precision P>
class TRSTransform {

public:
	TRSTransform();
	~TRSTransform();

public:
	// Reset the transformation to the identity matrix
	void ResetTransform();

	///////////////////////////////////////////////////////////////////////////
	// Relative transformations: combine new transform with existing ones.
	///////////////////////////////////////////////////////////////////////////

	void Translate(glm::tvec3<T, P> v);
	void Scale(glm::tvec3<T, P> v);
	void Scale(T uniform);

	// Rotate around vector (x, y, z)
	// - Perform `new = current * newRotation`
	void Rotate(T angle, glm::tvec3<T, P> v);
	void RotateX(T angle);
	void RotateY(T angle);
	void RotateZ(T angle);
	// - Perform `new = newRotation * current`
	void PreRotate(T angle, glm::tvec3<T, P> v);
	void PreRotateX(T angle);
	void PreRotateY(T angle);
	void PreRotateZ(T angle);


	///////////////////////////////////////////////////////////////////////////
	// Absolute transformations: overwrite existing transformations with new ones.
	///////////////////////////////////////////////////////////////////////////

	void SetTranslate(glm::tvec3<T, P> v);
	void SetScale(glm::tvec3<T, P> v);
	void SetScale(T uniform);

	// Rotate around vector (x, y, z)
	void SetRotate(T angle, glm::tvec3<T, P> v);
	void SetRotateX(T angle);
	void SetRotateY(T angle);
	void SetRotateZ(T angle);


	void LookTowards(glm::tvec3<T, P> front_vec, glm::tvec3<T, P> up_vec);
	void LookTowards(glm::tvec3<T, P> front_vec);
	void LookAt(glm::tvec3<T, P> point, glm::tvec3<T, P> up_vec);
	void LookAt(glm::tvec3<T, P> point);


	///////////////////////////////////////////////////////////////////////////
	// Useful getters
	///////////////////////////////////////////////////////////////////////////

	glm::tmat4x4<T, P> GetMatrix() const;
	glm::tmat4x4<T, P> GetMatrixInverse() const;

	glm::tmat3x3<T, P> GetRotation() const;
	glm::tvec3<T, P> GetTranslation() const;
	glm::tvec3<T, P> GetScale() const;

	glm::tmat4x4<T, P> GetTranslationMatrix() const;
	glm::tmat4x4<T, P> GetRotationMatrix() const;
	glm::tmat4x4<T, P> GetScaleMatrix() const;

	glm::tmat4x4<T, P> GetTranslationMatrixInverse() const;
	glm::tmat4x4<T, P> GetRotationMatrixInverse() const;
	glm::tmat4x4<T, P> GetScaleMatrixInverse() const;

	glm::tmat4x4<T, P> GetTranslationRotationMatrix() const;

	glm::tvec3<T, P> GetUp() const;
	glm::tvec3<T, P> GetDown() const;
	glm::tvec3<T, P> GetLeft() const;
	glm::tvec3<T, P> GetRight() const;
	glm::tvec3<T, P> GetFront() const;
	glm::tvec3<T, P> GetBack() const;

protected:
	glm::tmat3x3<T, P>	mR;
	glm::tvec3<T, P>	mT;
	glm::tvec3<T, P>	mS;

public:
	friend std::ostream &operator<<(std::ostream &os, TRSTransform<T, P> &v)
	{
		os << v.mT << std::endl;
		os << v.mR << std::endl;
		os << v.mS << std::endl;
		return os;
	}
	friend std::istream &operator>>(std::istream &is, TRSTransform<T, P> &v)
	{
		is >> v.mT;
		is >> v.mR;
		is >> v.mS;
		return is;
	}
};

#include "TRSTransform.inl"

using TRSTransformf = TRSTransform<float, glm::defaultp>;
using TRSTransformd = TRSTransform<double, glm::defaultp>;
