#pragma once

// launcher/common.hpp has to come first. it defines WIN32_LEAN_AND_MEAN, which
// keeps <windows.h> from pulling in the legacy <winsock.h> and clashing with the
// <winsock2.h> that <curl/curl.h> includes on windows.
#include "launcher/common.hpp"

#include <curl/curl.h>

#include <string>

namespace by {

    // RAII wrapper for a single libcurl easy handle.
    //
    // libcurl makes the caller drive the whole sequence by hand: create the handle,
    // set the options, perform the transfer, then clean it up. curl_global_init()
    // also has to be called once somewhere in the process. this class takes over
    // that lifetime management and leaves only downloadAsStr() and download().
    //
    // ```cpp
    //  curl c;
    //  std::string manifest;
    //  WHEN(!c.downloadAsStr(url, manifest)).err("%s", c.getErr().c_str()).ret(false);
    // ```
    class _nout curl {
        BY(ME(curl))

    public:
        curl();
        ~curl();

        // an easy handle has a single owner, so it can't be duplicated.
        curl(const me& rhs) = delete;
        me& operator=(const me& rhs) = delete;

    public:
        // whether the handle was acquired. every transfer fails when this is false.
        nbool isValid() const;

        // receives the content of url into out. used for small files like a manifest.
        // out is left empty when it fails.
        nbool downloadAsStr(const std::string& url, std::string& out);

        // receives the content of url into the file at path. used for big files like
        // a toolchain zip. the partial file is removed when it fails, so path exists
        // only on success.
        nbool download(const std::string& url, const std::string& path);

        // why the last transfer failed. empty when it succeeded.
        const std::string& getErr() const;

        // seconds allowed for 1 transfer. 0 means unlimited.
        void setTimeout(nint sec);
        nint getTimeout() const;

    private:
        // puts url and the options shared by every transfer on the handle. the caller
        // only has to set the write callback on top of it.
        void _setupCommon(const std::string& url);

        // runs curl_easy_perform() and leaves the failure reason on _err.
        nbool _run();

        void _rel();

    private:
        CURL* _handle;
        std::string _err;
        nint _timeout;
    };
}
