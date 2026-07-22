#pragma once

#ifdef _WIN32
#   include <windows.h>
#endif

#ifdef __APPLE__
#   include <mach-o/dyld.h>
#endif

#include <flagStacker.hpp>
#include <indep.hpp>
