#pragma once

// launcher/common.hpp has to come first. it defines WIN32_LEAN_AND_MEAN, which
// keeps <windows.h> from pulling in the legacy <winsock.h> and clashing with the
// <winsock2.h> that <curl/curl.h> includes on windows.
#include "launcher/common.hpp"

#include <curl/curl.h>

#include <string>

namespace by {

    /**
     * @brief RAII wrapper for a single libcurl easy handle
     * @details owns the handle and curl_global_init(), so a caller only picks where
     *          the content should land.
     * @code
     *  curl c;
     *  curl::res manifest = c.downloadAsStr(url);
     *  WHEN(!manifest.has()).err("%s", curl_easy_strerror(manifest.getErr())).ret(false);
     * @endcode
     */
    class _nout curl {
        BY(ME(curl))

    public:
        /**
         * @brief what 1 transfer gave back
         * @details carries the received content when it worked and the reason it
         *          stopped when it didn't, so a failure can't outlive the call that
         *          made it. curl_easy_strerror() turns the error into a message.
         */
        typedef tres<std::string, CURLcode> res;

    public:
        curl();
        ~curl();

        /** @brief an easy handle has a single owner, so it can't be duplicated. */
        curl(const me& rhs) = delete;
        me& operator=(const me& rhs) = delete;

    public:
        /** @brief whether the handle was acquired. every transfer fails when this is false. */
        nbool isValid() const;

        /**
         * @brief receives the content of url. used for small files like a manifest.
         * @return the whole content, or the CURLcode it stopped with.
         */
        res downloadAsStr(const std::string& url);

        /**
         * @brief receives the content of url into the file at path
         * @details used for big files like a toolchain zip. the partial file is
         *          removed when it fails, so path exists only on success.
         * @return path, or the CURLcode it stopped with.
         */
        res downloadAsFile(const std::string& url, const std::string& path);

    private:
        // the sink is all that separates one transfer from another, so it's the only
        // thing the two download functions have to bring.
        CURLcode _run(const std::string& url, curl_write_callback onWrite, void* sink);

        void _rel();

    private:
        CURL* _handle;
    };
}
