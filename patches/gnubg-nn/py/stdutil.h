// stdutil.h — basic string utilities for GNU BG
#pragma once

#if defined(_WIN32)
#include <stdlib.h>  // for _putenv_s, _strdup
  #include <string.h>  // for _stricmp, _strnicmp

  // Map POSIX names to MSVC equivalents
  #define strcasecmp  _stricmp
  #define strncasecmp _strnicmp
  #define strdup      _strdup
  // Shim setenv→_putenv_s
  #define setenv(name,val,overwrite) _putenv_s(name,val)
#endif

#include <assert.h>
#include <string.h>    // for strcmp, strncmp

/// Compares s1 and s2 exactly.
inline bool streq(const char* s1, const char* s2) {
    assert(s1 && s2);
    return strcmp(s1, s2) == 0;
}

/// Compares first n characters of s1 and s2 exactly.
inline bool strneq(const char* s1, const char* s2, int n) {
    assert(s1 && s2);
    return strncmp(s1, s2, n) == 0;
}

/// Compares s1 and s2, ignoring case.
inline bool strcaseeq(const char* s1, const char* s2) {
    assert(s1 && s2);
    return strcasecmp(s1, s2) == 0;
}

/// Compares first n characters of s1 and s2, ignoring case.
inline bool strncaseeq(const char* s1, const char* s2, int n) {
    assert(s1 && s2);
    return strncasecmp(s1, s2, n) == 0;
}
