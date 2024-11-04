#ifndef CHAR_DEFS_H
#define CHAR_DEFS_H

#ifdef _WIN32
typedef wchar_t CharType;
#define STR(x) L##x
#else
typedef char CharType;
#define STR(x) x
#endif

#ifdef __cplusplus
#include <string>

#ifdef _WIN32
typedef std::wstring StringType;
#else
typedef std::string StringType;
#endif

#endif  // __cplusplus
#endif  // CHAR_DEFS_H
