#include "Bonobo.h"
#include "Log.h"

Bonobo::Bonobo() {
	LogInfo("Framework initialisation done.");
}

Bonobo::~Bonobo() {
	LogInfo("Framework shutting down.");
}

WindowManager& Bonobo::GetWindowManager() noexcept
{
	return windowManager;
}

Bonobo::LogWrapper::LogWrapper()
{
	Log::Init();
}

Bonobo::LogWrapper::~LogWrapper()
{
	Log::Destroy();
}

Bonobo::LogViewWrapper::LogViewWrapper()
{
	Log::View::Init();
}

Bonobo::LogViewWrapper::~LogViewWrapper()
{
	Log::View::Destroy();
}
