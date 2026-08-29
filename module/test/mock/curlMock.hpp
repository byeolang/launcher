#pragma once

// the fake libcurl this binary links in place of the real one.
#include "launcher/installer/curl.hpp"

#include <gmock/gmock.h>

#include <string>

namespace by {

    /** @brief every option one transfer was configured with. */
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
     * @details free functions can't carry expectations, so they forward here.
     */
    class curlMock {
    public:
        MOCK_METHOD(CURL*, easyInit, ());
        MOCK_METHOD(CURLcode, easyPerform, (CURL*) );
        MOCK_METHOD(void, easyCleanup, (CURL*) );

    public:
        /**
         * @brief feeds content to the write callback, as a real transfer would
         * @return whether the callback took all of it.
         */
        nbool pump(const std::string& content);

    public:
        /** @brief what _run() configured. */
        curlOpts opts;

    public:
        static void bind(curlMock* mock);
        static curlMock* get();
    };

    /** @brief the handle curl_easy_init() hands out on success. */
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
