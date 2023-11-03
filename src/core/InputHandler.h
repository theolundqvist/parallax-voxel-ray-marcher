#pragma once

#include <array>
#include <cstdint>
#include <unordered_map>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <glm/vec2.hpp>

/*----------------------------------------------------------------------------*/

#define PRESSED						(1 << 0)
#define RELEASED					(1 << 1)
#define JUST_PRESSED				(1 << 2)
#define JUST_RELEASED				(1 << 3)

class InputHandler
{
public:
	struct IState {
		std::uint64_t mDownTick{ std::numeric_limits<std::uint64_t>::max() };
		std::uint64_t mUpTick{ std::numeric_limits<std::uint64_t>::max() };
		bool mIsDown{ false };
	};

public:
	InputHandler();

public:
	void FeedKeyboard(int key, int scancode, int action);
	void FeedMouseButtons(int button, int action);
	void FeedMouseMotion(glm::vec2 const& position);
	void Advance();
	std::uint32_t GetScancodeState(int scancode);
	std::uint32_t GetKeycodeState(int key);
	std::uint32_t GetMouseState(std::uint32_t button);
	glm::vec2 GetMousePositionAtStateShift(std::uint32_t button);
	glm::vec2 GetMousePosition();
	bool IsMouseCapturedByUI() const;
	bool IsKeyboardCapturedByUI() const;
	void SetUICapture(bool mouseCapture, bool keyboardCapture);

private:
	using InputStateMap = std::unordered_map<size_t, IState>;

	void DownEvent(InputStateMap& map, size_t loc);
	void UpEvent(InputStateMap& map, size_t loc);
	std::uint32_t GetState(InputStateMap const& map, size_t loc);

	InputStateMap mScancodeMap;
	InputStateMap mKeycodeMap;
	InputStateMap mMouseMap;

	glm::vec2 mMousePosition{ -1.0f };
	std::array<glm::vec2, GLFW_MOUSE_BUTTON_LAST> mMousePositionSwitched;

	bool mMouseCapturedByUI{ false };
	bool mKeyboardCapturedByUI{ false };

	std::uint64_t mTick{ 0ULL };

};

