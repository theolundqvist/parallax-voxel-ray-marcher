#pragma once

#include "TRSTransform.h"
#include "InputHandler.h"

#include <glm/glm.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/io.hpp>

#include <chrono>
#include <iostream>

template<typename T, glm::precision P>
class FPSCamera
{
public:
	FPSCamera(T fovy, T aspect, T nnear, T nfar);
	~FPSCamera();

public:
	void Update(std::chrono::microseconds deltaTime, InputHandler &ih, bool ignoreKeyEvents = false, bool ignoreMouseEvents = false);
	void SetProjection(T fovy, T aspect, T nnear, T nfar);
	void SetFov(T fovy);
	T GetFov();
	void SetAspect(T a);
	T GetAspect();

	glm::tmat4x4<T, P> GetViewToWorldMatrix();
	glm::tmat4x4<T, P> GetWorldToViewMatrix();
	glm::tmat4x4<T, P> GetClipToWorldMatrix();
	glm::tmat4x4<T, P> GetWorldToClipMatrix();
	glm::tmat4x4<T, P> GetClipToViewMatrix();
	glm::tmat4x4<T, P> GetViewToClipMatrix();

	glm::tvec3<T, P> GetClipToWorld(glm::tvec3<T, P> xyw);
	glm::tvec3<T, P> GetClipToView(glm::tvec3<T, P> xyw);

public:
	TRSTransform<T, P> mWorld;
	glm::tvec3<T, P> mMovementSpeed;
	glm::tvec2<T, P> mMouseSensitivity;

public:
	T mFov, mAspect, mNear, mFar;
	glm::tmat4x4<T, P> mProjection;
	glm::tmat4x4<T, P> mProjectionInverse;
	glm::tvec2<T, P> mMousePosition;

public:
	friend std::ostream &operator<<(std::ostream &os, FPSCamera<T, P> &v) {
		os << v.mFov << " " << v.mAspect << " " << v.mNear << " " << v.mFar << std::endl;
		os << v.mMovementSpeed << " " << v.mMouseSensitivity << std::endl;
		os << v.mWorld;
		return os;
	}
	friend std::istream &operator>>(std::istream &is, FPSCamera<T, P> &v) {
		T fov, aspect, nnear, far;
		is >> fov >> aspect >> nnear >> far;
		v.SetProjection(fov, aspect, nnear, far);
		is >> v.mMovementSpeed >> v.mMouseSensitivity;
		is >> v.mWorld;
		return is;
	}
};

#include "FPSCamera.inl"

typedef FPSCamera<float, glm::defaultp> FPSCameraf;
typedef FPSCamera<double, glm::defaultp> FPSCamerad;
