#pragma once

// WIN32_LEAN_AND_MEAN keeps <windows.h> from pulling in the legacy <winsock.h>,
// which would clash with the <winsock2.h> that <curl/curl.h> includes.
// NOMINMAX prevents the min/max macros from colliding with the standard library.
//
// these have to be defined before anything includes <windows.h>, so common.hpp
// includes this file ahead of dep.hpp.
#ifdef _WIN32
#   ifndef WIN32_LEAN_AND_MEAN
#      define WIN32_LEAN_AND_MEAN
#   endif
#   ifndef NOMINMAX
#      define NOMINMAX
#   endif
#endif
