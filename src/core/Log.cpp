#include "config.hpp"
#include "Log.h"
#include <cstring>
#include <cstdio>
#include <sstream>
#include <string>
#include <iostream>
#include <thread>
#include <unordered_map>
#ifdef _WIN32
#	include <Windows.h>
#endif
#include <mutex>

namespace Log {

#define RESULT_MAX_STRING_LENGTH	16384

FILE *logfile = nullptr;
void (* textout_func)(Type, const char *) = nullptr;
std::unordered_map<size_t, size_t> once_map;
size_t output_targets = LOG_OUT_STD | LOG_OUT_CUSTOM | LOG_OUT_FILE;
std::mutex fileMutex;
char log_result_string[RESULT_MAX_STRING_LENGTH];
bool logIncludeThreadID = false;

struct LogSettings {
	Type type;
	std::string prefix;
	Verbosity verbosity;
	Severity severity;
};

LogSettings logSettings[] = {
	{ TYPE_SUCCESS		, "Success: "			, LOUD_UNSITUATED	, OK		},
	{ TYPE_INFO			, ""					, LOUD_UNSITUATED	, OK		},
	{ TYPE_NEUTRAL		, ""					, LOUD_UNSITUATED	, OK		},
	{ TYPE_WARNING		, "Warning: "			, LOUD				, OK 		},
	{ TYPE_ERROR		, "Error: "				, LOUD				, BAD	},
	{ TYPE_FILE			, ""					, LOUD_UNSITUATED	, OK		},
	{ TYPE_ASSERT		, "Assert: "			, LOUD				, BAD		},
	{ TYPE_PARAM		, "Parameter error: "	, LOUD				, BAD		},
	{ TYPE_TRIVIA		, ""					, LOUD_UNSITUATED	, OK		}
};

/*----------------------------------------------------------------------------*/

void Init()
{
	SetOutputTargets(output_targets);
}

/*----------------------------------------------------------------------------*/

void Destroy()
{
	fileMutex.lock();
	if (!logfile) {
		fileMutex.unlock();
		return;
	}
	fprintf(logfile, "\n === End of log === \n\n");
	fflush(logfile);
	fclose(logfile);
	logfile = nullptr;
	fileMutex.unlock();
}

/*----------------------------------------------------------------------------*/

void SetCustomOutputTargetFunc(void (* textout)(Type, const char *))
{
	textout_func = textout;
}

/*----------------------------------------------------------------------------*/

void SetOutputTargets(size_t flags)
{
	output_targets = flags;
	if (flags & LOG_OUT_FILE) {
		fileMutex.lock();
		if (logfile == nullptr) {
			// Lazily initiate file
			logfile = fopen("log.txt", "w");
			if (!logfile) {
				fileMutex.unlock();
				perror("Failed to open \"log.txt\" for writing");
				fprintf(stderr, "Disabling logging of messages to file.\n");
				output_targets = output_targets & ~LOG_OUT_FILE;
				return;
			}
			fprintf(logfile, "\n === Log (%s, %s) === \n\n", __DATE__, __TIME__);
			fflush(logfile);
		}
		fileMutex.unlock();
	}
}

/*----------------------------------------------------------------------------*/

void SetVerbosity(Type type, Verbosity verbosity)
{
	logSettings[size_t(type)].verbosity = verbosity;
}

/*----------------------------------------------------------------------------*/

void SetIncludeThreadID(bool inc)
{
	logIncludeThreadID = inc;
}

/*----------------------------------------------------------------------------*/

void Report(
		unsigned int		flags,
		const char			*file,
		const char			*function,
		int					line,
		Type				type,
		const char			*str,
		...
	)
{
	if (output_targets == 0)
		return;
	size_t t = size_t(type);
#ifndef LOG_WHISPERS
	if (logSettings[t].verbosity == Verbosity::WHISPER)
		return;
#endif

	size_t len;
	va_list args;
	va_start(args, str);
	vsnprintf(log_result_string, RESULT_MAX_STRING_LENGTH - 1, str, args);
	va_end(args);
	len = strlen(log_result_string);
	if (len >= (RESULT_MAX_STRING_LENGTH - 1))
		strcat(&log_result_string[RESULT_MAX_STRING_LENGTH - 5], "...");

	if (flags != 0) {
		std::ostringstream os;
		if ((flags & LOG_MESSAGE_ONCE_FLAG) != 0)
			os << file << function << std::to_string(line) << log_result_string;
		if ((type & LOG_LOCATION_ONCE_FLAG) != 0)
			os << "_Loc" << file << function << std::to_string(line);
		std::hash<std::string> hash_func;
		size_t hash = hash_func(os.str());
		auto elem = once_map.find(hash);
		if (elem != once_map.end()) {
			elem->second++; // Count the number of hits
			return;
		}
		once_map[hash] = 1;
	}

	std::ostringstream os;
	if (logIncludeThreadID) {
		std::thread::id tid = std::this_thread::get_id();
		os << "{" << tid << "} ";
	}
	if (logSettings[t].verbosity == LOUD) {
		if (line == -1)
			os << "[Unknown location]" << std::endl;
		else
			os << "[" << file << ", " << function << " (" << std::to_string(line) << ")]" << std::endl;
	}
	os << logSettings[t].prefix << log_result_string << std::endl;

	if (output_targets & LOG_OUT_STD) {
#if defined(_WIN32)
		auto const widened_string = utils::widen(os.str());
		fwprintf(logSettings[t].severity != Severity::OK ? stderr : stdout, L"%s", widened_string.c_str());
#else
		fprintf(logSettings[t].severity != Severity::OK ? stderr : stdout, "%s", os.str().c_str());
#endif
	}
	if (output_targets & LOG_OUT_FILE) {
		fileMutex.lock();
		fprintf(logfile, "%s", os.str().c_str());
		fflush(logfile);
		fileMutex.unlock();
	}
	if (output_targets & LOG_OUT_CUSTOM && textout_func != nullptr)
		textout_func(type, os.str().c_str());
#ifdef _WIN32
	if (logSettings[t].severity != Severity::OK && IsDebuggerPresent())
  		__debugbreak();
#endif
	if (logSettings[t].severity == Severity::TERMINAL) {
		Destroy();
		exit(1); // TODO: Proper deconstruction
	}
}

/*----------------------------------------------------------------------------*/

bool ReportParam(
		unsigned int		test,
		const char			*file,
		const char			*function,
		int					line
	)
{
	bool t = test != 0;
	if (!t)	Report(0, file, function, line, Type::TYPE_PARAM, "Bad parameter!");
	return t;
}

/*----------------------------------------------------------------------------*/

};
