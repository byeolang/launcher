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
    // that lifetime management and leaves only downloadAsStr() and downloadAsFile().
    //
    // ```cpp
    //  curl c;
    //  curl::res manifest = c.downloadAsStr(url);
    //  WHEN(!manifest.has()).err("%s", curl_easy_strerror(manifest.getErr())).ret(false);
    // ```
    class _nout curl {
        BY(ME(curl))

    public:
        // what 1 transfer gave back. it carries the received content when it worked
        // and the reason it stopped when it didn't, so a failure can't outlive the
        // call that made it. curl_easy_strerror() turns the error into a message.
        typedef tres<std::string, CURLcode> res;

    public:
        curl();
        ~curl();

        // an easy handle has a single owner, so it can't be duplicated.
        curl(const me& rhs) = delete;
        me& operator=(const me& rhs) = delete;

    public:
        // whether the handle was acquired. every transfer fails when this is false.
        nbool isValid() const;

        // receives the content of url. used for small files like a manifest.
        res downloadAsStr(const std::string& url);

        // receives the content of url into the file at path. used for big files like
        // a toolchain zip, and gives path back once the file is whole. the partial
        // file is removed when it fails, so path exists only on success.
        res downloadAsFile(const std::string& url, const std::string& path);

    private:
        // wires url, the options shared by every transfer and the given sink onto the
        // handle, runs the transfer, then logs the reason when it fails. the sink is
        // all that separates one transfer from another, so it's the only thing the
        // two download functions have to bring.
        CURLcode _run(const std::string& url, curl_write_callback onWrite, void* sink);

        void _rel();

    private:
        CURL* _handle;
    };
}
