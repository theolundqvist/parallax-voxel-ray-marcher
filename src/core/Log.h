/*
 * Error and warning handling system
 */

#include "BuildSettings.h"

#include <cstddef>
#include <cstdarg>

#pragma once
//#define ENABLE_ASSERT		1
//#define ENABLE_PARAM_CHECK	1

#define SUCCESS(r)			((r) == RESULT_SUCCESS)
#define FAILURE(r)			(!SUCCESS(r))

#if defined ENABLE_ASSERT && ENABLE_ASSERT != 0
#	define Assert(m)		if (!(static_cast<unsigned int>(m))) Log::Report(0, __FILE__, __FUNCTION__, __LINE__, Log::Type::TYPE_ASSERT, "Assertion failed at line %u in file %s.", __LINE__, __FILE__)
#else
#	define Assert(a)
#endif

#if defined ENABLE_PARAM_CHECK && ENABLE_PARAM_CHECK != 0
#	define Param(m)			Log::ReportParam(static_cast<unsigned int>(m), __FILE__, __FUNCTION__, __LINE__)
#else
#	define Param(a)			true
#endif

#define LOG_MESSAGE_ONCE_FLAG		1
#define LOG_LOCATION_ONCE_FLAG		2

namespace Log {

enum Type {
	TYPE_SUCCESS		= 0,
	TYPE_INFO			= 1,
	TYPE_NEUTRAL		= 2,
	TYPE_WARNING		= 3,
	TYPE_ERROR			= 4,
	TYPE_FILE			= 5,
	TYPE_ASSERT			= 6,
	TYPE_PARAM			= 7,
	TYPE_TRIVIA			= 8,
	N_TYPES				= 9
};

enum Severity {
	OK = 0,
	BAD,
	TERMINAL
};
enum Verbosity {
	WHISPER = 0,		// Disregard message
	LOUD_UNSITUATED,	// Display message
	LOUD				// Display message, with file, function and line prepended
};

#define LOG_OUT_STD		(1 << 0)
#define LOG_OUT_FILE	(1 << 1)
#define LOG_OUT_CUSTOM	(1 << 15)

void Init();
void Destroy();
void SetCustomOutputTargetFunc(void (* textout)(Type, const char *));
void SetOutputTargets(std::size_t targets);
void SetVerbosity(Type type, Verbosity verbosity);
void SetIncludeThreadID(bool inc);

/** Report a result to a log file and standard output */
void Report(
		unsigned int		flags,
		const char			*file,
		const char			*function,
		int					line,
		Type				type,
		const char			*str,
		...
	);

bool ReportParam(
		unsigned int		test,
		const char			*file,
		const char			*function,
		int					line
	);

};

//__forceinline Log::Type operator|(Log::Type l, Log::Type r)
//{
//	return static_cast<Log::Type>(static_cast<int>(l) | static_cast<int>(r));
//}

#if defined _WIN32
#	define Log(m, ...)				Log::Report(0, __FILE__, __FUNCTION__, __LINE__, Log::Type::TYPE_NEUTRAL, static_cast<char const*>(m), __VA_ARGS__)
#	define LogType(t, m, ...)		Log::Report(0, __FILE__, __FUNCTION__, __LINE__, t, static_cast<char const*>(m), __VA_ARGS__)
#	define LogMsgOnce(t, m, ...)	Log::Report(LOG_MESSAGE_ONCE_FLAG, __FILE__, __FUNCTION__, __LINE__, t, static_cast<char const*>(m), __VA_ARGS__)
#	define LogLocOnce(t, m, ...)	Log::Report(LOG_LOCATION_ONCE_FLAG, __FILE__, __FUNCTION__, __LINE__, t, static_cast<char const*>(m), __VA_ARGS__)
#	define LogWarning(m, ...)		Log::Report(0, __FILE__, __FUNCTION__, __LINE__, Log::Type::TYPE_WARNING, static_cast<char const*>(m), __VA_ARGS__)
#	define LogError(m, ...)			Log::Report(0, __FILE__, __FUNCTION__, __LINE__, Log::Type::TYPE_ERROR, static_cast<char const*>(m), __VA_ARGS__)
#	define LogFile(m, ...)			Log::Report(0, __FILE__, __FUNCTION__, __LINE__, Log::Type::TYPE_FILE, static_cast<char const*>(m), __VA_ARGS__)
#	define LogInfo(m, ...)			Log::Report(0, __FILE__, __FUNCTION__, __LINE__, Log::Type::TYPE_INFO, static_cast<char const*>(m), __VA_ARGS__)
#	define LogTrivia(m, ...)		Log::Report(0, __FILE__, __FUNCTION__, __LINE__, Log::Type::TYPE_TRIVIA, static_cast<char const*>(m), __VA_ARGS__)
#else
#	define Log(m, ...)				Log::Report(0, __FILE__, __FUNCTION__, __LINE__, Log::Type::TYPE_NEUTRAL, static_cast<char const*>(m), ##__VA_ARGS__)
#	define LogType(t, m, ...)		Log::Report(0, __FILE__, __FUNCTION__, __LINE__, t, static_cast<char const*>(m), ##__VA_ARGS__)
#	define LogMsgOnce(t, m, ...)	Log::Report(LOG_MESSAGE_ONCE_FLAG, __FILE__, __FUNCTION__, __LINE__, t, static_cast<char const*>(m), ##__VA_ARGS__)
#	define LogLocOnce(t, m, ...)	Log::Report(LOG_LOCATION_ONCE_FLAG, __FILE__, __FUNCTION__, __LINE__, t, static_cast<char const*>(m), ##__VA_ARGS__)
#	define LogWarning(m, ...)		Log::Report(0, __FILE__, __FUNCTION__, __LINE__, Log::Type::TYPE_WARNING, static_cast<char const*>(m), ##__VA_ARGS__)
#	define LogError(m, ...)			Log::Report(0, __FILE__, __FUNCTION__, __LINE__, Log::Type::TYPE_ERROR, static_cast<char const*>(m), ##__VA_ARGS__)
#	define LogFile(m, ...)			Log::Report(0, __FILE__, __FUNCTION__, __LINE__, Log::Type::TYPE_FILE, static_cast<char const*>(m), ##__VA_ARGS__)
#	define LogInfo(m, ...)			Log::Report(0, __FILE__, __FUNCTION__, __LINE__, Log::Type::TYPE_INFO, static_cast<char const*>(m), ##__VA_ARGS__)
#	define LogTrivia(m, ...)		Log::Report(0, __FILE__, __FUNCTION__, __LINE__, Log::Type::TYPE_TRIVIA, static_cast<char const*>(m), ##__VA_ARGS__)
#endif
