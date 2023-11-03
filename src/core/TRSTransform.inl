#include <cmath>
#include "TRSTransform.h"

#include <glm/gtc/matrix_transform.hpp>

/*----------------------------------------------------------------------------*/

template<typename T, glm::precision P>
TRSTransform<T, P>::TRSTransform()
{
	ResetTransform();
}

/*----------------------------------------------------------------------------*/

template<typename T, glm::precision P>
TRSTransform<T, P>::~TRSTransform()
{
}

/*----------------------------------------------------------------------------*/

template<typename T, glm::precision P>
void TRSTransform<T, P>::ResetTransform()
{
	mT = glm::tvec3<T, P>(static_cast<T>(0));
	mS = glm::tvec3<T, P>(static_cast<T>(1));
	mR = glm::tmat3x3<T, P>(static_cast<T>(1));
}

/*----------------------------------------------------------------------------*/

template<typename T, glm::precision P>
void TRSTransform<T, P>::Translate(glm::tvec3<T, P> v)
{
	mT += v;
}

/*----------------------------------------------------------------------------*/

template<typename T, glm::precision P>
void TRSTransform<T, P>::Scale(glm::tvec3<T, P> v)
{
	mS *= v;
}

/*----------------------------------------------------------------------------*/

template<typename T, glm::precision P>
void TRSTransform<T, P>::Scale(T uniform)
{
	mS *= uniform;
}

/*----------------------------------------------------------------------------*/

template<typename T, glm::precision P>
void TRSTransform<T, P>::Rotate(T angle, glm::tvec3<T, P> v)
{
	mR = glm::tmat3x3<T, P>(glm::rotate(glm::tmat4x4<T, P>(mR), angle, v));
}

/*----------------------------------------------------------------------------*/

template<typename T, glm::precision P>
void TRSTransform<T, P>::RotateX(T angle)
{
	T C = std::cos(angle);
	T S = std::sin(angle);
	mR = glm::tmat3x3<T, P>(
		mR[0][0], C * mR[0][1] - mR[0][2] * S, C * mR[0][2] + mR[0][1] * S,
		mR[1][0], C * mR[1][1] - mR[1][2] * S, C * mR[1][2] + mR[1][1] * S,
		mR[2][0], C * mR[2][1] - mR[2][2] * S, C * mR[2][2] + mR[2][1] * S);
}

/*----------------------------------------------------------------------------*/

template<typename T, glm::precision P>
void TRSTransform<T, P>::RotateY(T angle)
{
	T C = std::cos(angle);
	T S = std::sin(angle);
	mR = glm::tmat3x3<T, P>(
		C * mR[0][0] + mR[0][2] * S, mR[0][1], C * mR[0][2] - mR[0][0] * S,
		C * mR[1][0] + mR[1][2] * S, mR[1][1], C * mR[1][2] - mR[1][0] * S,
		C * mR[2][0] + mR[2][2] * S, mR[2][1], C * mR[2][2] - mR[2][0] * S);
}

/*----------------------------------------------------------------------------*/

template<typename T, glm::precision P>
void TRSTransform<T, P>::RotateZ(T angle)
{
	T C = std::cos(angle);
	T S = std::sin(angle);
	mR = glm::tmat3x3<T, P>(
		C * mR[0][0] - mR[0][1] * S, C * mR[0][1] + mR[0][0] * S, mR[0][2],
		C * mR[1][0] - mR[1][1] * S, C * mR[1][1] + mR[1][0] * S, mR[1][2],
		C * mR[2][0] - mR[2][1] * S, C * mR[2][1] + mR[2][0] * S, mR[2][2]);
}

/*----------------------------------------------------------------------------*/

template<typename T, glm::precision P>
void TRSTransform<T, P>::PreRotate(T angle, glm::tvec3<T, P> v)
{
	mR = glm::tmat3x3<T, P>::RotationMatrix(angle, v) * mR;
}

/*----------------------------------------------------------------------------*/

template<typename T, glm::precision P>
void TRSTransform<T, P>::PreRotateX(T angle)
{
	T C = cos(angle);
	T S = sin(angle);
	mR = glm::tmat3x3<T, P>(
		mR[0][0], mR[0][1], mR[0][2],
		C * mR[1][0] + mR[2][0] * S, C * mR[1][1] + mR[2][1] * S, C * mR[1][2] + mR[2][2] * S,
		C * mR[2][0] - mR[1][0] * S, C * mR[2][1] - mR[1][1] * S, C * mR[2][2] - mR[1][2] * S);
}

/*----------------------------------------------------------------------------*/

template<typename T, glm::precision P>
void TRSTransform<T, P>::PreRotateY(T angle)
{
	T C = cos(angle);
	T S = sin(angle);
	mR = glm::tmat3x3<T, P>(
		C * mR[0][0] - mR[2][0] * S, C * mR[0][1] - mR[2][1] * S, C * mR[0][2] - mR[2][2] * S,
		mR[1][0], mR[1][1], mR[1][2],
		C * mR[2][0] + mR[0][0] * S, C * mR[2][1] + mR[0][1] * S, C * mR[2][2] + mR[0][2] * S);
}

/*----------------------------------------------------------------------------*/

template<typename T, glm::precision P>
void TRSTransform<T, P>::PreRotateZ(T angle)
{
	T C = cos(angle);
	T S = sin(angle);
	mR = glm::tmat3x3<T, P>(
		C * mR[0][0] + mR[1][0] * S, C * mR[0][1] + mR[1][1] * S, C * mR[0][2] + mR[1][2] * S,
		C * mR[1][0] - mR[0][0] * S, C * mR[1][1] - mR[0][1] * S, C * mR[1][2] - mR[0][2] * S,
		mR[2][0], mR[2][1], mR[2][2]);
}

/*----------------------------------------------------------------------------*/

template<typename T, glm::precision P>
void TRSTransform<T, P>::SetTranslate(glm::tvec3<T, P> v)
{
	mT = v;
}

/*----------------------------------------------------------------------------*/

template<typename T, glm::precision P>
void TRSTransform<T, P>::SetScale(glm::tvec3<T, P> v)
{
	mS = v;
}

/*----------------------------------------------------------------------------*/

template<typename T, glm::precision P>
void TRSTransform<T, P>::SetScale(T uniform)
{
	mS = glm::tvec3<T, P>(uniform);
}

/*----------------------------------------------------------------------------*/

template<typename T, glm::precision P>
void TRSTransform<T, P>::SetRotate(T angle, glm::tvec3<T, P> v)
{
	mR = glm::tmat3x3<T, P>(glm::rotate(glm::tmat4x4<T, P>(T(1)), angle, v));
}

/*----------------------------------------------------------------------------*/

template<typename T, glm::precision P>
void TRSTransform<T, P>::SetRotateX(T angle)
{
	mR = glm::tmat3x3<T, P>(glm::rotate(glm::tmat4x4<T, P>(T(1)), angle, glm::tvec3<T, P>(1, 0, 0)));
}

/*----------------------------------------------------------------------------*/

template<typename T, glm::precision P>
void TRSTransform<T, P>::SetRotateY(T angle)
{
	mR = glm::tmat3x3<T, P>(glm::rotate(glm::tmat4x4<T, P>(T(1)), angle, glm::tvec3<T, P>(0, 1, 0)));
}

/*----------------------------------------------------------------------------*/

template<typename T, glm::precision P>
void TRSTransform<T, P>::SetRotateZ(T angle)
{
	mR = glm::tmat3x3<T, P>(glm::rotate(glm::tmat4x4<T, P>(T(1)), angle, glm::tvec3<T, P>(0, 0, 1)));
}

/*----------------------------------------------------------------------------*/

template<typename T, glm::precision P>
void TRSTransform<T, P>::LookTowards(glm::tvec3<T, P> front_vec, glm::tvec3<T, P> up_vec)
{
	front_vec = normalize(front_vec);
	up_vec = normalize(up_vec);

	if (std::abs(dot(up_vec, front_vec)) > 0.99999f)
		return;

	glm::tvec3<T, P> prev_up = up_vec;

	glm::tvec3<T, P> right = cross(front_vec, prev_up);
	glm::tvec3<T, P> up = cross(right, front_vec);

	right = normalize(right);
	up = normalize(up);

	mR[0] = right;
	mR[1] = up;
	mR[2] = -front_vec;
}

/*----------------------------------------------------------------------------*/

template<typename T, glm::precision P>
void TRSTransform<T, P>::LookTowards(glm::tvec3<T, P> front_vec)
{
	LookTowards(front_vec, glm::tvec3<T, P>(0, 1, 0));
}

/*----------------------------------------------------------------------------*/

template<typename T, glm::precision P>
void TRSTransform<T, P>::LookAt(glm::tvec3<T, P> point, glm::tvec3<T, P> up_vec)
{
	LookTowards(point - mT, up_vec);
}

/*----------------------------------------------------------------------------*/

template<typename T, glm::precision P>
void TRSTransform<T, P>::LookAt(glm::tvec3<T, P> point)
{
	LookTowards(point - mT);
}

/*----------------------------------------------------------------------------*/

template<typename T, glm::precision P>
glm::tmat4x4<T, P> TRSTransform<T, P>::GetTranslationMatrix() const
{
	return glm::tmat4x4<T, P>(
			1, 0, 0, 0,
			0, 1, 0, 0,
			0, 0, 1, 0,
			mT.x, mT.y, mT.z, 1);
}

/*----------------------------------------------------------------------------*/

template<typename T, glm::precision P>
glm::tmat4x4<T, P> TRSTransform<T, P>::GetRotationMatrix() const
{
	return glm::tmat4x4<T, P>(
			mR[0][0], mR[0][1], mR[0][2], 0,
			mR[1][0], mR[1][1], mR[1][2], 0,
			mR[2][0], mR[2][1], mR[2][2], 0,
			0  , 0  , 0  , 1);
}

/*----------------------------------------------------------------------------*/

template<typename T, glm::precision P>
glm::tmat4x4<T, P> TRSTransform<T, P>::GetScaleMatrix() const
{
	return glm::tmat4x4<T, P>(
			mS.x, 0  , 0  , 0,
			0  , mS.y, 0  , 0,
			0  , 0  , mS.z, 0,
			0  , 0  , 0  , 1);
}

/*----------------------------------------------------------------------------*/

template<typename T, glm::precision P>
glm::tmat4x4<T, P> TRSTransform<T, P>::GetTranslationMatrixInverse() const
{
	return glm::tmat4x4<T, P>(
			1, 0, 0, 0,
			0, 1, 0, 0,
			0, 0, 1, 0,
			-mT.x, -mT.y, -mT.z, 1);
}

/*----------------------------------------------------------------------------*/

template<typename T, glm::precision P>
glm::tmat4x4<T, P> TRSTransform<T, P>::GetRotationMatrixInverse() const
{
	return glm::tmat4x4<T, P>(
			mR[0][0], mR[1][0], mR[2][0], 0,
			mR[0][1], mR[1][1], mR[2][1], 0,
			mR[0][2], mR[1][2], mR[2][2], 0,
			0, 0, 0, 1);
}

/*----------------------------------------------------------------------------*/

template<typename T, glm::precision P>
glm::tmat4x4<T, P> TRSTransform<T, P>::GetScaleMatrixInverse() const
{
	return glm::tmat4x4<T, P>(
			T(1)/mS.x, 0, 0, 0,
			0, T(1)/mS.y, 0, 0,
			0, 0, T(1)/mS.z, 0,
			0, 0, 0, 1);
}

/*----------------------------------------------------------------------------*/

template<typename T, glm::precision P>
glm::tmat4x4<T, P> TRSTransform<T, P>::GetTranslationRotationMatrix() const
{
	return glm::tmat4x4<T, P>(
			mR[0][0], mR[0][1], mR[0][2], 0,
			mR[1][0], mR[1][1], mR[1][2], 0,
			mR[2][0], mR[2][1], mR[2][2], 0,
			mT.x, mT.y, mT.z, 1);
}

/*----------------------------------------------------------------------------*/

template<typename T, glm::precision P>
glm::tmat4x4<T, P> TRSTransform<T, P>::GetMatrix() const
{
	return glm::tmat4x4<T, P>(
			mR[0][0]*mS.x, mR[0][1]*mS.x, mR[0][2]*mS.x, 0,
			mR[1][0]*mS.y, mR[1][1]*mS.y, mR[1][2]*mS.y, 0,
			mR[2][0]*mS.z, mR[2][1]*mS.z, mR[2][2]*mS.z, 0,
			mT.x, mT.y, mT.z, 1);
}

/*----------------------------------------------------------------------------*/

template<typename T, glm::precision P>
glm::tmat4x4<T, P> TRSTransform<T, P>::GetMatrixInverse() const
{
	glm::tvec3<T, P> X = glm::tvec3<T, P>(T(1) / mS.x, T(1) / mS.y, T(1) / mS.z);

	T a = mR[0][0] * X.x;
	T b = mR[1][0] * X.y;
	T c = mR[2][0] * X.z;
	T d = mR[0][1] * X.x;
	T e = mR[1][1] * X.y;
	T f = mR[2][1] * X.z;
	T g = mR[0][2] * X.x;
	T h = mR[1][2] * X.y;
	T i = mR[2][2] * X.z;

	return glm::tmat4x4<T, P>(
			a, b, c, 0,
			d, e, f, 0,
			g, h, i, 0,
			-(mT.x * a + mT.y * d + mT.z * g), -(mT.x * b + mT.y * e + mT.z * h), -(mT.x * c + mT.y * f + mT.z * i), 1);
}

/*----------------------------------------------------------------------------*/

template<typename T, glm::precision P>
glm::tmat3x3<T, P> TRSTransform<T, P>::GetRotation() const
{
	return mR;
}

/*----------------------------------------------------------------------------*/

template<typename T, glm::precision P>
glm::tvec3<T, P> TRSTransform<T, P>::GetTranslation() const
{
	return mT;
}

/*----------------------------------------------------------------------------*/

template<typename T, glm::precision P>
glm::tvec3<T, P> TRSTransform<T, P>::GetScale() const
{
	return mS;
}

/*----------------------------------------------------------------------------*/

template<typename T, glm::precision P>
glm::tvec3<T, P> TRSTransform<T, P>::GetUp() const
{
	return glm::tvec3<T, P>(mR[1][0]*mS.y, mR[1][1]*mS.y, mR[1][2]*mS.y);
}

template<typename T, glm::precision P>
glm::tvec3<T, P> TRSTransform<T, P>::GetDown() const
{
	return -GetUp();
}

template<typename T, glm::precision P>
glm::tvec3<T, P> TRSTransform<T, P>::GetLeft() const
{
	return -GetRight();
}

template<typename T, glm::precision P>
glm::tvec3<T, P> TRSTransform<T, P>::GetRight() const
{
	return glm::tvec3<T, P>(mR[0][0]*mS.x, mR[0][1]*mS.x, mR[0][2]*mS.x);
}

template<typename T, glm::precision P>
glm::tvec3<T, P> TRSTransform<T, P>::GetFront() const
{
	return -GetBack();
}

template<typename T, glm::precision P>
glm::tvec3<T, P> TRSTransform<T, P>::GetBack() const
{
	return glm::tvec3<T, P>(mR[2][0]*mS.z, mR[2][1]*mS.z, mR[2][2]*mS.z);
}

/*----------------------------------------------------------------------------*/
