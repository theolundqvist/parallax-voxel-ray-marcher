#include "InputHandler.h"

/*----------------------------------------------------------------------------*/

InputHandler::InputHandler()
{
	for (auto& mousePosition : mMousePositionSwitched)
	{
		mousePosition = mMousePosition;
	}
}

void InputHandler::Advance()
{
	mTick++;
}

void InputHandler::DownEvent(InputStateMap& map, size_t loc)
{
	// If the key already exists, insert does not modify the associated value
	// and just returns an iterator to it.
	auto insertionResult = map.insert({ loc, IState() });
	auto& state = insertionResult.first->second;
	state.mIsDown = true;
	state.mDownTick = mTick;
}

void InputHandler::UpEvent(InputStateMap& map, size_t loc)
{
	// If the key already exists, insert does not modify the associated value
	// and just returns an iterator to it.
	auto insertionResult = map.insert({ loc, IState() });
	auto& state = insertionResult.first->second;
	state.mIsDown = false;
	state.mUpTick = mTick;
}

void InputHandler::FeedKeyboard(int key, int scancode, int action)
{
	switch (action)
	{
		case GLFW_PRESS:
			DownEvent(mScancodeMap, static_cast<size_t>(scancode));
			DownEvent(mKeycodeMap, static_cast<size_t>(key));
			break;
		case GLFW_RELEASE:
			UpEvent(mScancodeMap, static_cast<size_t>(scancode));
			UpEvent(mKeycodeMap, static_cast<size_t>(key));
			break;
		default:
			break;
	}
}

void InputHandler::FeedMouseMotion(glm::vec2 const& position)
{
	mMousePosition = position;
}

void InputHandler::FeedMouseButtons(int button, int action)
{
	switch (action)
	{
		case GLFW_PRESS:
			DownEvent(mMouseMap, static_cast<size_t>(button));
			break;
		case GLFW_RELEASE:
			UpEvent(mMouseMap, static_cast<size_t>(button));
			break;
		default:
			break;
	}
	mMousePositionSwitched[button] = mMousePosition;
}

std::uint32_t InputHandler::GetState(InputStateMap const& map, size_t loc)
{
	auto const sc = map.find(loc);
	if (sc == map.end())
		return RELEASED;

	auto const& state = sc->second;
	std::uint32_t s = state.mIsDown ? PRESSED : RELEASED;
	s |= mTick-1 == state.mDownTick ? JUST_PRESSED : 0;
	s |= mTick-1 == state.mUpTick ? JUST_RELEASED : 0;

	return s;
}

std::uint32_t InputHandler::GetScancodeState(int scancode)
{
	return GetState(mScancodeMap, static_cast<size_t>(scancode));
}

std::uint32_t InputHandler::GetKeycodeState(int  key)
{
	return GetState(mKeycodeMap, static_cast<size_t>(key));
}

std::uint32_t InputHandler::GetMouseState(std::uint32_t button)
{
	return GetState(mMouseMap, static_cast<size_t>(button));
}

glm::vec2 InputHandler::GetMousePositionAtStateShift(std::uint32_t button)
{
	return mMousePositionSwitched[button];
}

glm::vec2 InputHandler::GetMousePosition()
{
	return mMousePosition;
}

bool InputHandler::IsMouseCapturedByUI() const
{
	return mMouseCapturedByUI;
}

bool InputHandler::IsKeyboardCapturedByUI() const
{
	return mKeyboardCapturedByUI;
}

void InputHandler::SetUICapture(bool mouseCapture, bool keyboardCapture)
{
	mMouseCapturedByUI = mouseCapture;
	mKeyboardCapturedByUI = keyboardCapture;
}
