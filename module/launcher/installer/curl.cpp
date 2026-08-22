#include "launcher/installer/curl.hpp"

#include <filesystem>
#include <fstream>

namespace by {

    // default limit for 1 transfer, in seconds. a toolchain zip is tens of MB.
    static constexpr nint DEFAULT_TIMEOUT = 300;

    // separate limit for the connection only, in seconds. it cuts off a server that
    // never answers instead of waiting out DEFAULT_TIMEOUT.
    static constexpr nint CONNECT_TIMEOUT = 15;

    // the github API rejects a request without a User-Agent with 403.
    static constexpr const nchar* USER_AGENT = "byeol-launcher";

    BY(DEF_ME(curl))

    namespace {
        // curl_global_init() is needed once per process. it initializes when the first
        // curl object is made and cleans up when the program ends. a function local
        // static makes the compiler guarantee that it happens only once.
        class globalCurl {
        public:
            globalCurl() { curl_global_init(CURL_GLOBAL_DEFAULT); }
            ~globalCurl() { curl_global_cleanup(); }
        };

        void _initGlobal() { static globalCurl inner; }

        // appends the received bytes to the string.
        std::size_t _onWriteToStr(nchar* ptr, std::size_t size, std::size_t nmemb, void* out) {
            std::string* dest = (std::string*) out;
            dest->append(ptr, size * nmemb);
            return size * nmemb;
        }

        // writes the received bytes to the file. when writing fails, like on a full
        // disk, it returns a length different from the consumed one to make curl
        // abort the transfer.
        std::size_t _onWriteToFile(nchar* ptr, std::size_t size, std::size_t nmemb, void* out) {
            std::ofstream* dest = (std::ofstream*) out;
            dest->write(ptr, (std::streamsize) (size * nmemb));
            if(!*dest) return 0;
            return size * nmemb;
        }
    }

    me::curl(): _handle(nullptr), _timeout(DEFAULT_TIMEOUT) {
        // a ctor can't use the WHEN macro. isValid() reports a failed handle instead.
        _initGlobal();
        _handle = curl_easy_init();
    }

    me::~curl() { _rel(); }

    nbool me::isValid() const { return _handle != nullptr; }

    nbool me::downloadAsStr(const std::string& url, std::string& out) {
        out.clear();
        WHEN(!isValid()).err("curl handle isn't ready.").ret(false);

        _setupCommon(url);
        curl_easy_setopt(_handle, CURLOPT_WRITEFUNCTION, _onWriteToStr);
        curl_easy_setopt(_handle, CURLOPT_WRITEDATA, &out);
        WHEN(_run()).ret(true);

        // leaving the head of a failed transfer makes the caller take it for a whole
        // response.
        out.clear();
        return false;
    }

    nbool me::download(const std::string& url, const std::string& path) {
        WHEN(!isValid()).err("curl handle isn't ready.").ret(false);

        std::ofstream file(path, std::ios::binary);
        WHEN(!file).err("failed to open %s to write.", path.c_str()).ret(false);

        _setupCommon(url);
        curl_easy_setopt(_handle, CURLOPT_WRITEFUNCTION, _onWriteToFile);
        curl_easy_setopt(_handle, CURLOPT_WRITEDATA, &file);
        WHEN(_run()).ret(true);

        // leaving a half received file makes the next run take it for a whole one.
        // windows can't remove an open file, so it closes first.
        file.close();
        std::error_code ec;
        std::filesystem::remove(path, ec);
        return false;
    }

    const std::string& me::getErr() const { return _err; }

    void me::setTimeout(nint sec) { _timeout = sec; }

    nint me::getTimeout() const { return _timeout; }

    void me::_setupCommon(const std::string& url) {
        _err.clear();

        curl_easy_setopt(_handle, CURLOPT_URL, url.c_str());
        // a github release redirects to another host to serve the real file.
        curl_easy_setopt(_handle, CURLOPT_FOLLOWLOCATION, 1L);
        // without this the body of a 404 gets stored as if it were real content.
        curl_easy_setopt(_handle, CURLOPT_FAILONERROR, 1L);
        // keeps the timeout off signals. it makes calling from a thread safe.
        curl_easy_setopt(_handle, CURLOPT_NOSIGNAL, 1L);
        curl_easy_setopt(_handle, CURLOPT_TIMEOUT, (long) _timeout);
        curl_easy_setopt(_handle, CURLOPT_CONNECTTIMEOUT, (long) CONNECT_TIMEOUT);
        curl_easy_setopt(_handle, CURLOPT_USERAGENT, USER_AGENT);
    }

    nbool me::_run() {
        CURLcode res = curl_easy_perform(_handle);
        WHEN(res == CURLE_OK).ret(true);

        _err = curl_easy_strerror(res);
        BY_E("transfer failed: %s", _err.c_str());
        return false;
    }

    void me::_rel() {
        WHEN(!isValid()).ret();

        curl_easy_cleanup(_handle);
        _handle = nullptr;
    }
}
