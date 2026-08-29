#pragma once

// the fake libcurl this binary links in place of the real one. curl.cpp compiles
// untouched; the linker resolves its curl_easy_* symbols to the definitions in
// curlMock.cc, which forward to the curlMock bound for the running test.
#include "launcher/installer/curl.hpp"

#include <gmock/gmock.h>

#include <string>

namespace by {

    /**
     * @brief every option one transfer was configured with
     * @details curl::_run() pushes these through curl_easy_setopt(), so a test
     *          reads back what the wrapper asked for instead of watching the wire.
     */
    struct curlOpts {
        std::string url;
        std::string userAgent;
        nlong followLocation = -1;
        nlong failOnError = -1;
        nlong noSignal = -1;
        nlong connectTimeout = -1;
        nlong lowSpeedLimit = -1;
        nlong lowSpeedTime = -1;
        curl_write_callback onWrite = nullptr;
        void* sink = nullptr;
    };

    /**
     * @brief the seam the faked curl_easy_* functions delegate to
     * @details the fakes are free functions and can't carry expectations, so they
     *          forward to the instance bound by scopedCurlMock.
     */
    class curlMock {
    public:
        MOCK_METHOD(CURL*, easyInit, ());
        MOCK_METHOD(CURLcode, easyPerform, (CURL*) );
        MOCK_METHOD(void, easyCleanup, (CURL*) );

    public:
        /**
         * @brief hands content to the write callback curl.cpp registered
         * @details what makes a faked perform() look like a real transfer: the
         *          sink fills exactly as libcurl would fill it. call it from an
         *          easyPerform() action to model a transfer that received data.
         * @return whether the callback took all of it, which is how libcurl
         *         decides a sink refused the write.
         */
        nbool pump(const std::string& content);

    public:
        /** @brief what _run() configured. */
        curlOpts opts;

    public:
        // the fakes are free functions, so they need one well known place to look.
        static void bind(curlMock* mock);
        static curlMock* get();
    };

    /** @brief the handle curl_easy_init() hands out when a test lets it succeed. */
    CURL* aCurlHandle();

    /** @brief binds a mock for one test and unbinds it on the way out. */
    class scopedCurlMock {
    public:
        scopedCurlMock() { curlMock::bind(&_mock); }

        ~scopedCurlMock() { curlMock::bind(nullptr); }

    public:
        curlMock& operator*() { return _mock; }

        curlMock* operator->() { return &_mock; }

    private:
        curlMock _mock;
    };
} // namespace by
