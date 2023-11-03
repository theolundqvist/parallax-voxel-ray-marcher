#include "various.hpp"

#include "core/Log.h"

#include <fstream>
#include <iostream>
#include <limits>
#include <memory>
#if defined(_WIN32)
#include <Windows.h>
#endif

#if defined(_WIN32)
// Implementation based on this article by Giovanni Dicanio:
// https://docs.microsoft.com/en-us/archive/msdn-magazine/2016/september/c-unicode-encoding-conversions-with-stl-strings-and-win32-apis

std::wstring
utils::widen(char const* utf8)
{
	std::wstring utf16;
	if (!utf8 || utf8[0] == '\0') {
		return utf16;
	}

	int const utf16_length = ::MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, utf8, -1, nullptr, 0);
	if (utf16_length == 0) {
		LogError("Failed to retrieve the length of the resulting UTF-16 string; MultiByteToWideChar generated the error code %d.", ::GetLastError());
		return utf16;
	}
	utf16.resize(utf16_length);

	int const result = ::MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, utf8, -1, &utf16[0], utf16_length);
	if (result == 0) {
		LogError("Conversion from UTF-8 to UTF-16 failed; MultiByteToWideChar generated the error code %d.", ::GetLastError());
		return utf16;
	}

	return utf16;
}

std::wstring
utils::widen(std::string const& utf8)
{
	std::wstring utf16;
	if (utf8.empty()) {
		return utf16;
	}

	if (utf8.length() > static_cast<size_t>((std::numeric_limits<int>::max)())) {
		LogError("The length of \"%s\" is too long to fit in an int, which is needed for the conversion to UTF-16.", utf8.c_str());
		return utf16;
	}
	int const utf8_length = static_cast<int>(utf8.length());

	int const utf16_length = ::MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, utf8.data(), utf8_length, nullptr, 0);
	if (utf16_length == 0) {
		LogError("Failed to retrieve the length of the resulting UTF-16 string; MultiByteToWideChar generated the error code %d.", ::GetLastError());
		return utf16;
	}
	utf16.resize(utf16_length);

	int const result = ::MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, utf8.data(), utf8_length, &utf16[0], utf16_length);
	if (result == 0) {
		LogError("Conversion from UTF-8 to UTF-16 failed; MultiByteToWideChar generated the error code %d.", ::GetLastError());
		return utf16;
	}

	return utf16;
}
#endif

std::string
utils::slurp_file(std::string const& path)
{
  std::ifstream file = std::ifstream(utils::widen(path));
  if (!file.is_open()) {
    LogError("Failed to open \"%s\"", path.c_str());
    return std::string("");
  }

  file.seekg(0, std::ios::end);
  auto const size = file.tellg();
  file.seekg(0, std::ios::beg);

  std::unique_ptr<char[]> content = std::make_unique<char[]>(static_cast<size_t>(size) + 1ll);
  file.read(content.get(), size);
  content[static_cast<size_t>(size)] = '\0';

  return std::string(content.get());
}
