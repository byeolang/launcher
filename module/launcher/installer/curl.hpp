#pragma once

#include "launcher/common.hpp"

#include <string>

namespace by {

    // libcurl의 easy handle 1개를 감싸는 RAII 래퍼.
    //
    // libcurl은 handle 생성, 옵션 설정, 전송, 해제를 호출자가 직접 순서대로
    // 다뤄야 하고, curl_global_init()도 프로그램 어딘가에서 한 번 불러줘야 한다.
    // 이 클래스는 그 수명 관리를 전부 가져가서, 호출자에게는 get()과 download()
    // 두 개만 남긴다.
    //
    // <curl/curl.h>를 헤더에 노출하지 않는 것도 목적 중 하나다. 그 헤더는 windows
    // 에서 <winsock2.h>를 끌어오기 때문에 include 순서를 신경써야 하는데,
    // 이 클래스를 거치면 그 제약이 curl.cpp 안에만 머문다.
    //
    // ```cpp
    //  curl c;
    //  std::string manifest;
    //  WHEN(!c.get(url, manifest)).err("%s", c.getErr().c_str()).ret(false);
    // ```
    class _nout curl {
        BY(ME(curl))

    public:
        curl();
        ~curl();

        // easy handle은 소유권이 하나뿐이라 복제할 수 없다. 이동만 허용한다.
        curl(const me& rhs) = delete;
        curl(me&& rhs) noexcept;
        me& operator=(const me& rhs) = delete;
        me& operator=(me&& rhs) noexcept;

    public:
        // handle을 확보했는지 여부. false면 모든 전송이 실패한다.
        nbool isValid() const;

        // url의 내용을 out에 받는다. manifest처럼 작은 파일에 쓴다.
        // 실패하면 out은 비워진 상태로 남는다.
        nbool get(const std::string& url, std::string& out);

        // url의 내용을 path 파일로 받는다. toolchain zip처럼 큰 파일에 쓴다.
        // 실패하면 반쯤 받은 파일을 지우므로, 성공한 경우에만 path가 존재한다.
        nbool download(const std::string& url, const std::string& path);

        // 마지막 전송이 실패한 이유. 성공했다면 비어있다.
        const std::string& getErr() const;

        // 전송 1회에 허용할 시간(초). 0이면 무제한.
        void setTimeout(nint sec);
        nint getTimeout() const;

    private:
        // url과 전송마다 동일한 옵션을 handle에 올린다. 호출부는 write 콜백만
        // 따로 지정하면 된다.
        void _setup(const std::string& url);

        // curl_easy_perform()을 돌리고 실패 사유를 _err에 남긴다.
        nbool _run();

        void _rel();

    private:
        // CURL*. 헤더에 <curl/curl.h>를 들이지 않으려고 void*로 들고 있다.
        void* _handle;
        std::string _err;
        nint _timeout;
    };
}
