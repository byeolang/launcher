#include "mock/curlMock.hpp"

#include <cstdarg>

namespace by {

    namespace {
        curlMock* _bound = nullptr;

        // CURL is opaque to curl.cpp, so an address is all a handle has to be.
        nint _handle = 0;
    }

    void curlMock::bind(curlMock* mock) { _bound = mock; }

    curlMock* curlMock::get() { return _bound; }

    nbool curlMock::pump(const std::string& content) {
        if(!opts.onWrite) return false;

        // libcurl calls the callback with (size, nmemb); a size of 1 makes nmemb
        // the byte count, which is what it does for a received body.
        std::size_t took = opts.onWrite((nchar*) content.data(), 1, content.size(), opts.sink);
        return took == content.size();
    }

    CURL* aCurlHandle() { return (CURL*) &_handle; }
}

// ---- the fake libcurl ------------------------------------------------------
//
// these carry the signatures from <curl/easy.h> so the linker takes them for the
// real thing. an object file wins over an archive member, so these are picked
// even while the real libcurl sits on the link line.

extern "C" {

    CURLcode curl_global_init(long) { return CURLE_OK; }

    void curl_global_cleanup() {}

    CURL* curl_easy_init() {
        by::curlMock* mock = by::curlMock::get();
        if(!mock) return nullptr;
        return mock->easyInit();
    }

    CURLcode curl_easy_perform(CURL* handle) {
        by::curlMock* mock = by::curlMock::get();
        if(!mock) return CURLE_FAILED_INIT;
        return mock->easyPerform(handle);
    }

    void curl_easy_cleanup(CURL* handle) {
        by::curlMock* mock = by::curlMock::get();
        if(mock) mock->easyCleanup(handle);
    }

    const char* curl_easy_strerror(CURLcode code) {
        // the launcher only ever prints this, so a stable string is enough.
        return code == CURLE_OK ? "ok" : "faked curl error";
    }

    // gmock can't take a va_list, so each option is unpacked here and only the
    // value it actually carries lands on curlOpts.
    //
    // the signature has to match <curl/easy.h> exactly for the linker to accept
    // this in libcurl's place, which puts an enum last before the ellipsis.
    // va_start() on a type that undergoes default argument promotion is formally
    // undefined, and the project builds with -Werror, so the diagnostic is turned
    // off for just this function -- every libcurl caller relies on the same thing.
#if defined(__clang__)
#    pragma clang diagnostic push
#    pragma clang diagnostic ignored "-Wvarargs"
#elif defined(__GNUC__)
#    pragma GCC diagnostic push
#    pragma GCC diagnostic ignored "-Wvarargs"
#endif
    CURLcode curl_easy_setopt(CURL*, CURLoption option, ...) {
        by::curlMock* mock = by::curlMock::get();
        if(!mock) return CURLE_FAILED_INIT;

        va_list args;
        va_start(args, option);

        by::curlOpts& opts = mock->opts;
        switch(option) {
            case CURLOPT_URL: opts.url = va_arg(args, const char*); break;
            case CURLOPT_USERAGENT: opts.userAgent = va_arg(args, const char*); break;
            case CURLOPT_FOLLOWLOCATION: opts.followLocation = va_arg(args, long); break;
            case CURLOPT_FAILONERROR: opts.failOnError = va_arg(args, long); break;
            case CURLOPT_NOSIGNAL: opts.noSignal = va_arg(args, long); break;
            case CURLOPT_CONNECTTIMEOUT: opts.connectTimeout = va_arg(args, long); break;
            case CURLOPT_LOW_SPEED_LIMIT: opts.lowSpeedLimit = va_arg(args, long); break;
            case CURLOPT_LOW_SPEED_TIME: opts.lowSpeedTime = va_arg(args, long); break;
            case CURLOPT_WRITEFUNCTION: opts.onWrite = va_arg(args, curl_write_callback); break;
            case CURLOPT_WRITEDATA: opts.sink = va_arg(args, void*); break;
            default: break;
        }

        va_end(args);
        return CURLE_OK;
    }
#if defined(__clang__)
#    pragma clang diagnostic pop
#elif defined(__GNUC__)
#    pragma GCC diagnostic pop
#endif
}
