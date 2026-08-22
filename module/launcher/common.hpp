#pragma once

// def.hpp comes first: it defines WIN32_LEAN_AND_MEAN before dep.hpp includes
// <windows.h>, which is what keeps the legacy <winsock.h> out of the way of the
// <winsock2.h> that <curl/curl.h> needs.
#include "common/def.hpp"
#include "common/dep.hpp"
