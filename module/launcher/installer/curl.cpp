#include "launcher/installer/curl.hpp"

#include <filesystem>
#include <fstream>

namespace by {

    // cuts off a server that never answers before a single byte is on the wire.
    static constexpr nint CONNECT_TIMEOUT = 15;

    // a transfer gives up once its average speed stays under LOW_SPEED_LIMIT bytes per
    // second for LOW_SPEED_TIME seconds. the total time is left uncapped on purpose: a
    // toolchain zip is tens of MB, so any wall clock cap short enough to catch a dead
    // server also kills a slow link that is doing nothing wrong.
    static constexpr nint LOW_SPEED_LIMIT = 1024;
    static constexpr nint LOW_SPEED_TIME = 30;

    // the github API rejects a request without a User-Agent with 403.
    static constexpr const nchar* USER_AGENT = "byeol-launcher";

    BY(DEF_ME(curl))

    namespace {
        // curl_global_init() is needed once per process. a function local static makes
        // the compiler guarantee that it happens only once.
        class globalCurl {
        public:
            globalCurl() { curl_global_init(CURL_GLOBAL_DEFAULT); }
            ~globalCurl() { curl_global_cleanup(); }
        };

        void _initGlobal() { static globalCurl inner; }

        std::size_t _onWriteToStr(nchar* ptr, std::size_t size, std::size_t nmemb, void* out) {
            std::string* dest = (std::string*) out;
            dest->append(ptr, size * nmemb);
            return size * nmemb;
        }

        // returning a length different from the consumed one is what makes curl abort
        // the transfer, like on a full disk.
        std::size_t _onWriteToFile(nchar* ptr, std::size_t size, std::size_t nmemb, void* out) {
            std::ofstream* dest = (std::ofstream*) out;
            dest->write(ptr, (std::streamsize) (size * nmemb));
            if(!*dest) return 0;
            return size * nmemb;
        }
    }

    me::curl(): _handle(nullptr) {
        // a ctor can't use the WHEN macro. isValid() reports a failed handle instead.
        _initGlobal();
        _handle = curl_easy_init();
    }

    me::~curl() { _rel(); }

    nbool me::isValid() const { return _handle != nullptr; }

    me::res me::downloadAsStr(const std::string& url) {
        WHEN(!isValid()).err("curl handle isn't ready.").ret(res(CURLE_FAILED_INIT));

        // the head of a cut transfer stays on out and gets dropped with it. handing it
        // over would make the caller take it for a whole response.
        std::string out;
        CURLcode code = _run(url, _onWriteToStr, &out);
        WHEN(code != CURLE_OK).ret(res(code));
        return res(out);
    }

    me::res me::downloadAsFile(const std::string& url, const std::string& path) {
        WHEN(!isValid()).err("curl handle isn't ready.").ret(res(CURLE_FAILED_INIT));

        std::ofstream file(path, std::ios::binary);
        WHEN(!file).err("failed to open %s to write.", path.c_str()).ret(res(CURLE_WRITE_ERROR));

        CURLcode code = _run(url, _onWriteToFile, &file);
        // the buffered tail is still on its way out, and losing it leaves the file short
        // just like a cut transfer does. windows also can't remove an open file, so
        // closing here covers the removal below too.
        file.close();
        if(code == CURLE_OK && !file) code = CURLE_WRITE_ERROR;
        WHEN(code == CURLE_OK).ret(res(path));

        // leaving a half received file makes the next run take it for a whole one.
        std::error_code ec;
        std::filesystem::remove(path, ec);
        return res(code);
    }

    CURLcode me::_run(const std::string& url, curl_write_callback onWrite, void* sink) {
        curl_easy_setopt(_handle, CURLOPT_URL, url.c_str());
        // a github release redirects to another host to serve the real file.
        curl_easy_setopt(_handle, CURLOPT_FOLLOWLOCATION, 1L);
        // without this the body of a 404 gets stored as if it were real content.
        curl_easy_setopt(_handle, CURLOPT_FAILONERROR, 1L);
        // keeps the timeouts off signals. it makes calling from a thread safe.
        curl_easy_setopt(_handle, CURLOPT_NOSIGNAL, 1L);
        curl_easy_setopt(_handle, CURLOPT_CONNECTTIMEOUT, (long) CONNECT_TIMEOUT);
        curl_easy_setopt(_handle, CURLOPT_LOW_SPEED_LIMIT, (long) LOW_SPEED_LIMIT);
        curl_easy_setopt(_handle, CURLOPT_LOW_SPEED_TIME, (long) LOW_SPEED_TIME);
        curl_easy_setopt(_handle, CURLOPT_USERAGENT, USER_AGENT);
        curl_easy_setopt(_handle, CURLOPT_WRITEFUNCTION, onWrite);
        curl_easy_setopt(_handle, CURLOPT_WRITEDATA, sink);

        CURLcode code = curl_easy_perform(_handle);
        WHEN(code != CURLE_OK).err("transfer failed: %s", curl_easy_strerror(code)).ret(code);
        return code;
    }

    void me::_rel() {
        WHEN(!isValid()).ret();

        curl_easy_cleanup(_handle);
        _handle = nullptr;
    }
}
