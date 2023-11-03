// TODO: Timestamp

#pragma once

#include "Log.h"

#define BUFFER_WIDTH	512
#define BUFFER_ROWS		64

namespace Log {

class View {
public:
	static void Init();
	static void Destroy();
public:
	static void Render();

private:
	static void Feed(Log::Type type, const char *msg);
	static void ClearLog();

private:
	static char mOutput[BUFFER_ROWS * BUFFER_WIDTH * 2];
	static char mBuffer[BUFFER_ROWS][BUFFER_WIDTH];
	static int mLen[BUFFER_ROWS];
	static Log::Type mType[BUFFER_ROWS];
	static int mBufferPtr;
	static bool mAutoScroll;
	static bool mScrollToBottom;
};

}
