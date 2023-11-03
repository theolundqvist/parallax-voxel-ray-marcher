#pragma once


#include <string>


namespace utils
{

#if defined(_WIN32)
std::wstring widen(char const* utf8);
std::wstring widen(std::string const& utf8);
#else
inline char const* widen(char const* utf8) { return utf8; }
inline std::string const& widen(std::string const& utf8) { return utf8; }
#endif

std::string slurp_file(std::string const& path);

} // end of namespace
