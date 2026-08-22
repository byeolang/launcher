#include "launcher/installer/curl.hpp"

#include <clog/when.hpp>

#include <curl/curl.h>

#include <filesystem>
#include <fstream>

namespace by {

    // 전송 1회의 기본 제한시간(초). toolchain zip이 수십 MB인 것을 감안한 값이다.
    static constexpr nint DEFAULT_TIMEOUT = 300;

    // 연결까지만 따로 두는 제한시간(초). 서버가 응답하지 않을 때 DEFAULT_TIMEOUT을
    // 다 기다리지 않고 끊기 위한 값이다.
    static constexpr nint CONNECT_TIMEOUT = 15;

    // github API는 User-Agent가 없는 요청을 403으로 막는다.
    static constexpr const nchar* USER_AGENT = "byeol-launcher";

    BY(DEF_ME(curl))

    namespace {
        // curl_global_init()은 프로세스당 한 번이면 된다. curl 객체가 처음 만들어질
        // 때 초기화하고 프로그램이 끝날 때 정리한다. 함수 지역 static이라 초기화가
        // 한 번만 일어나는 것을 컴파일러가 보장한다.
        class globalCurl {
        public:
            globalCurl() { curl_global_init(CURL_GLOBAL_DEFAULT); }
            ~globalCurl() { curl_global_cleanup(); }
        };

        void _initGlobal() { static globalCurl inner; }

        // 받은 바이트를 문자열 뒤에 이어붙인다.
        std::size_t _onWriteToStr(char* ptr, std::size_t size, std::size_t nmemb, void* out) {
            std::string* dest = static_cast<std::string*>(out);
            dest->append(ptr, size * nmemb);
            return size * nmemb;
        }

        // 받은 바이트를 파일에 쓴다. 디스크가 가득 찬 경우처럼 쓰기가 실패하면 소비한
        // 길이와 다른 값을 반환해서 curl이 전송을 중단하게 한다.
        std::size_t _onWriteToFile(char* ptr, std::size_t size, std::size_t nmemb, void* out) {
            std::ofstream* dest = static_cast<std::ofstream*>(out);
            dest->write(ptr, static_cast<std::streamsize>(size * nmemb));
            if(!*dest) return 0;
            return size * nmemb;
        }
    }

    me::curl(): _handle(nullptr), _timeout(DEFAULT_TIMEOUT) {
        // 생성자는 WHEN 매크로를 쓸 수 없다. handle 확보 실패는 isValid()로 알린다.
        _initGlobal();
        _handle = curl_easy_init();
    }

    me::~curl() { _rel(); }

    me::curl(me&& rhs) noexcept: _handle(rhs._handle), _err(std::move(rhs._err)), _timeout(rhs._timeout) {
        rhs._handle = nullptr;
    }

    me& me::operator=(me&& rhs) noexcept {
        if(this == &rhs) return *this;

        _rel();
        _handle = rhs._handle;
        _err = std::move(rhs._err);
        _timeout = rhs._timeout;
        rhs._handle = nullptr;
        return *this;
    }

    nbool me::isValid() const { return _handle != nullptr; }

    nbool me::get(const std::string& url, std::string& out) {
        out.clear();
        WHEN(!isValid()).err("curl handle isn't ready.").ret(false);

        _setup(url);
        CURL* handle = static_cast<CURL*>(_handle);
        curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, _onWriteToStr);
        curl_easy_setopt(handle, CURLOPT_WRITEDATA, &out);
        WHEN(_run()).ret(true);

        // 실패한 전송의 앞부분만 남아있으면 호출부가 온전한 응답으로 오해한다.
        out.clear();
        return false;
    }

    nbool me::download(const std::string& url, const std::string& path) {
        WHEN(!isValid()).err("curl handle isn't ready.").ret(false);

        std::ofstream file(path, std::ios::binary);
        if(!file) _err = "failed to open " + path + " to write.";
        WHEN(!file).err("%s", _err.c_str()).ret(false);

        _setup(url);
        CURL* handle = static_cast<CURL*>(_handle);
        curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, _onWriteToFile);
        curl_easy_setopt(handle, CURLOPT_WRITEDATA, &file);
        WHEN(_run()).ret(true);

        // 반쯤 받은 파일을 남기면 다음 실행이 그걸 온전한 파일로 오해한다.
        // windows는 열려있는 파일을 지울 수 없으므로 닫고 나서 지운다.
        file.close();
        std::error_code ec;
        std::filesystem::remove(path, ec);
        return false;
    }

    const std::string& me::getErr() const { return _err; }

    void me::setTimeout(nint sec) { _timeout = sec; }

    nint me::getTimeout() const { return _timeout; }

    void me::_setup(const std::string& url) {
        _err.clear();

        CURL* handle = static_cast<CURL*>(_handle);
        curl_easy_setopt(handle, CURLOPT_URL, url.c_str());
        // github release는 실제 파일을 다른 호스트로 리다이렉트해서 내려준다.
        curl_easy_setopt(handle, CURLOPT_FOLLOWLOCATION, 1L);
        // 이게 없으면 404 응답 본문이 정상 내용인 것처럼 저장된다.
        curl_easy_setopt(handle, CURLOPT_FAILONERROR, 1L);
        // 제한시간 구현에 signal을 쓰지 않게 한다. 스레드에서 불러도 안전해진다.
        curl_easy_setopt(handle, CURLOPT_NOSIGNAL, 1L);
        curl_easy_setopt(handle, CURLOPT_TIMEOUT, static_cast<long>(_timeout));
        curl_easy_setopt(handle, CURLOPT_CONNECTTIMEOUT, static_cast<long>(CONNECT_TIMEOUT));
        curl_easy_setopt(handle, CURLOPT_USERAGENT, USER_AGENT);
    }

    nbool me::_run() {
        CURLcode res = curl_easy_perform(static_cast<CURL*>(_handle));
        if(res != CURLE_OK) _err = curl_easy_strerror(res);
        WHEN(res != CURLE_OK).err("transfer failed: %s", _err.c_str()).ret(false);
        return true;
    }

    void me::_rel() {
        WHEN(!isValid()).ret();

        curl_easy_cleanup(static_cast<CURL*>(_handle));
        _handle = nullptr;
    }
}
