#pragma once

#include "core/Log.h"
#include "core/LogView.h"
#include "core/WindowManager.hpp"

class Bonobo {
public:
	Bonobo();
	~Bonobo();
	WindowManager& GetWindowManager() noexcept;

private:
	struct LogWrapper {
		LogWrapper();
		~LogWrapper();
	} logWrapper;
	WindowManager windowManager;
	struct LogViewWrapper {
		LogViewWrapper();
		~LogViewWrapper();
	} logViewWrapper;
};
