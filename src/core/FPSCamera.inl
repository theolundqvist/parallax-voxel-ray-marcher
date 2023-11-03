template<typename T, glm::precision P>
FPSCamera<T, P>::FPSCamera(T fovy, T aspect, T nnear, T nfar) : mWorld(), mMovementSpeed(1), mMouseSensitivity(1), mFov(fovy), mAspect(aspect), mNear(nnear), mFar(nfar), mProjection(), mProjectionInverse(), mMousePosition(glm::tvec2<T, P>(0.0f))
{
	SetProjection(fovy, aspect, nnear, nfar);
}

template<typename T, glm::precision P>
FPSCamera<T, P>::~FPSCamera()
{
}

template<typename T, glm::precision P>
void FPSCamera<T, P>::SetProjection(T fovy, T aspect, T nnear, T nfar)
{
	mFov = fovy;
	mAspect = aspect;
	mNear = nnear;
	mFar = nfar;
	mProjection = glm::perspective(fovy, aspect, nnear, nfar);
	mProjectionInverse = glm::inverse(mProjection);
}

template<typename T, glm::precision P>
void FPSCamera<T, P>::SetFov(T fovy)
{
	SetProjection(fovy, mAspect, mNear, mFar);
}

template<typename T, glm::precision P>
T FPSCamera<T, P>::GetFov()
{
	return mFov;
}

template<typename T, glm::precision P>
void FPSCamera<T, P>::SetAspect(T a)
{
	SetProjection(mFov, a, mNear, mFar);
}

template<typename T, glm::precision P>
T FPSCamera<T, P>::GetAspect()
{
	return mAspect;
}


template<typename T, glm::precision P>
void FPSCamera<T, P>::Update(std::chrono::microseconds deltaTime, InputHandler &ih, bool ignoreKeyEvents, bool ignoreMouseEvents)
{
	glm::tvec2<T, P> newMousePosition = glm::tvec2<T, P>(ih.GetMousePosition().x, ih.GetMousePosition().y);
	glm::tvec2<T, P> mouse_diff = newMousePosition - mMousePosition;
	mMousePosition = newMousePosition;

	if (!ih.IsMouseCapturedByUI() && !ignoreMouseEvents && (ih.GetMouseState(GLFW_MOUSE_BUTTON_LEFT) & PRESSED)) {
		mouse_diff.y = -mouse_diff.y;
		mouse_diff *= mMouseSensitivity;

		mWorld.PreRotateX(mouse_diff.y);
		mWorld.RotateY(-mouse_diff.x);
	}

	if (!ih.IsKeyboardCapturedByUI() && !ignoreKeyEvents) {
		T move = 0.0f, strafe = 0.0f, levitate = 0.0f;
		if ((ih.GetKeycodeState(GLFW_KEY_W) & PRESSED)) move += 1.0f;
		if ((ih.GetKeycodeState(GLFW_KEY_S) & PRESSED)) move -= 1.0f;
		if ((ih.GetKeycodeState(GLFW_KEY_A) & PRESSED)) strafe -= 1.0f;
		if ((ih.GetKeycodeState(GLFW_KEY_D) & PRESSED)) strafe += 1.0f;
		if ((ih.GetKeycodeState(GLFW_KEY_Q) & PRESSED)) levitate -= 1.0f;
		if ((ih.GetKeycodeState(GLFW_KEY_E) & PRESSED)) levitate += 1.0f;

		T const movementModifier = ((ih.GetKeycodeState(GLFW_KEY_LEFT_CONTROL) & PRESSED)) ? 0.25f : ((ih.GetKeycodeState(GLFW_KEY_LEFT_SHIFT) & PRESSED)) ? 4.0f : 1.0f;
		auto const deltaTime_s = std::chrono::duration<T>(deltaTime);
		auto const movementChange = movementModifier * (mWorld.GetFront() * move + mWorld.GetRight() * strafe + mWorld.GetUp() * levitate);
		auto const movementSpeed = mMovementSpeed * movementChange;

		mWorld.Translate(movementSpeed * deltaTime_s.count());
	}
}

template<typename T, glm::precision P>
glm::tmat4x4<T, P> FPSCamera<T, P>::GetViewToWorldMatrix()
{
	return mWorld.GetMatrix();
}

template<typename T, glm::precision P>
glm::tmat4x4<T, P> FPSCamera<T, P>::GetWorldToViewMatrix()
{
	return mWorld.GetMatrixInverse();
}

template<typename T, glm::precision P>
glm::tmat4x4<T, P> FPSCamera<T, P>::GetClipToWorldMatrix()
{
	return GetViewToWorldMatrix() * mProjectionInverse;
}

template<typename T, glm::precision P>
glm::tmat4x4<T, P> FPSCamera<T, P>::GetWorldToClipMatrix()
{
	return mProjection * GetWorldToViewMatrix();
}

template<typename T, glm::precision P>
glm::tmat4x4<T, P> FPSCamera<T, P>::GetClipToViewMatrix()
{
	return mProjectionInverse;
}

template<typename T, glm::precision P>
glm::tmat4x4<T, P> FPSCamera<T, P>::GetViewToClipMatrix()
{
	return mProjection;
}

template<typename T, glm::precision P>
glm::tvec3<T, P> FPSCamera<T, P>::GetClipToWorld(glm::tvec3<T, P> xyw)
{
	glm::tvec4<T, P> vv = glm::tvec4<T, P>(GetClipToView(xyw), static_cast<T>(1));
	glm::tvec3<T, P> wv = mWorld.GetMatrix() * vv;
	return wv;
}

template<typename T, glm::precision P>
glm::tvec3<T, P> FPSCamera<T, P>::GetClipToView(glm::tvec3<T, P> xyw)
{
	return xyw * glm::tvec3<T, P>(mProjectionInverse[0][0], mProjectionInverse[1][1], static_cast<T>(-1));
}
