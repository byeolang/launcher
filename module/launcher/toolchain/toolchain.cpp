#include "toolchain.hpp"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <set>
#include <stdexcept>
#include <system_error>

#ifndef _WIN32
#   include <limits.h>
#   include <sys/stat.h>
#endif

#include "mz.h"
#include "mz_zip.h"
#include "mz_zip_rw.h"
#include <curl/curl.h>

namespace fs = std::filesystem;

namespace {

#if defined(_WIN32)
    constexpr const char* kOs            = "win";
    constexpr const char* kArch          = "x64";
    constexpr const char* kWorkerExeName = "byeol.exe";
#elif defined(__APPLE__)
    constexpr const char* kOs = "macos";
#   if defined(__arm64__) || defined(__aarch64__)
    constexpr const char* kArch = "arm64";
#   else
    constexpr const char* kArch = "x64";
#   endif
    constexpr const char* kWorkerExeName = "byeol";
#else
    constexpr const char* kOs            = "ubuntu";
    constexpr const char* kArch          = "x64";
    constexpr const char* kWorkerExeName = "byeol";
#endif

    const std::string kGithubReleaseBase = "https://github.com/byeolang/byeol/releases/download/v";

    std::size_t curlWriteCallback(char* ptr, std::size_t size, std::size_t nmemb, void* userdata) {
        auto* out = static_cast<std::ofstream*>(userdata);
        out->write(ptr, static_cast<std::streamsize>(size * nmemb));
        if(!*out) return 0;
        return size * nmemb;
    }

    int curlProgressCallback(void*, curl_off_t dltotal, curl_off_t dlnow, curl_off_t, curl_off_t) {
        if(dltotal <= 0) return 0;
        int pct = static_cast<int>(dlnow * 100 / dltotal);
        std::cout << "\r  " << pct << "% (" << dlnow / 1024 << " / " << dltotal / 1024 << " KB)   "
                  << std::flush;
        return 0;
    }

} // namespace

namespace by {

    BY(DEF_ME(toolchain))

    me::toolchain(layout lay): _layout(std::move(lay)) {}

    const me::layout& me::getLayout() const { return _layout; }

    std::string me::currentVersion() const {
        std::ifstream input(_layout.activeToolchainFile);
        if(input) {
            std::string version;
            std::getline(input, version);
            version = _trim(version);
            if(!version.empty()) return version;
        }

        if(_isWorkerFile(_layout.bundledWorkerPath)) return "local";

        auto installed = list();
        if(!installed.empty()) return installed.front().version;
        return {};
    }

    fs::path me::resolveWorkerPath(const std::string& version) const {
        if(version.empty()) return {};

        if(version == "local" && _isWorkerFile(_layout.bundledWorkerPath))
            return _layout.bundledWorkerPath;

        fs::path bundled = _layout.bundledToolchainsDir / version / _layout.workerFileName;
        if(_isWorkerFile(bundled)) return bundled;

        fs::path userManaged = _layout.userToolchainsDir / version / _layout.workerFileName;
        if(_isWorkerFile(userManaged)) return userManaged;

        return {};
    }

    bool me::use(const std::string& version) {
        if(resolveWorkerPath(version).empty()) return false;

        _ensureHomeLayout();
        std::ofstream output(_layout.activeToolchainFile, std::ios::trunc);
        if(!output) throw std::runtime_error("failed to write active toolchain metadata");
        output << version << "\n";
        return true;
    }

    std::vector<me::entry> me::list() const {
        std::vector<entry> entries;
        std::set<std::string> seen;

        std::string activeVersion;
        {
            std::ifstream input(_layout.activeToolchainFile);
            if(input) {
                std::getline(input, activeVersion);
                activeVersion = _trim(activeVersion);
            }
        }
        if(activeVersion.empty() && _isWorkerFile(_layout.bundledWorkerPath))
            activeVersion = "local";

        auto addEntry = [&](const std::string& version, const fs::path& workerPath, bool bundled) {
            if(version.empty() || workerPath.empty()) return;
            if(!seen.insert(version).second) return;
            entries.push_back(entry{version, workerPath, version == activeVersion, bundled});
        };

        if(fs::exists(_layout.bundledToolchainsDir)) {
            for(const auto& e: fs::directory_iterator(_layout.bundledToolchainsDir)) {
                if(!e.is_directory()) continue;
                fs::path worker = e.path() / _layout.workerFileName;
                if(!_isWorkerFile(worker)) continue;
                addEntry(e.path().filename().string(), worker, true);
            }
        }

        if(fs::exists(_layout.userToolchainsDir)) {
            for(const auto& e: fs::directory_iterator(_layout.userToolchainsDir)) {
                if(!e.is_directory()) continue;
                fs::path worker = e.path() / _layout.workerFileName;
                if(!_isWorkerFile(worker)) continue;
                addEntry(e.path().filename().string(), worker, false);
            }
        }

        std::sort(entries.begin(), entries.end(),
                  [](const entry& lhs, const entry& rhs) { return lhs.version < rhs.version; });
        return entries;
    }

    int me::install(const std::string& version) {
        if(!_isSafeVersionString(version)) {
            std::cerr << "invalid version: " << version << "\n";
            return 1;
        }

        if(!resolveWorkerPath(version).empty()) {
            std::cout << "toolchain " << version << " is already installed.\n";
            return 0;
        }

        const std::string url = _buildDownloadUrl(version);
        std::cout << "platform: " << kOs << "-" << kArch << "\n";
        std::cout << "url:      " << url << "\n";

        const fs::path tmpZip =
            fs::temp_directory_path() / ("byeol-" + version + "-portable.zip");

        std::cout << "downloading...\n";
        if(!_downloadFile(url, tmpZip)) {
            std::cerr << "download failed. check version or platform availability.\n";
            return 1;
        }

        const fs::path installDir = _layout.bundledToolchainsDir / version;
        std::cout << "extracting to: " << installDir.string() << "\n";

        std::error_code ec;
        fs::create_directories(installDir, ec);

        const bool extracted = _extractZip(tmpZip, installDir);
        fs::remove(tmpZip, ec);

        if(!extracted) {
            std::cerr << "extraction failed.\n";
            fs::remove_all(installDir, ec);
            return 1;
        }

        const fs::path workerPath = installDir / kWorkerExeName;
        if(!_isWorkerFile(workerPath)) {
            std::cerr << "worker not found after extraction: " << workerPath << "\n";
            return 1;
        }

        std::cout << "installed: " << workerPath.string() << "\n";
        return 0;
    }

    me::layout me::makeLayout(const char* argv0) {
        layout lay;
        lay.executablePath = _detectExecutablePath(argv0);
        lay.executableDir  = lay.executablePath.parent_path();
        lay.workerFileName = kWorkerExeName;

        const char* home = std::getenv("HOME");
#ifdef _WIN32
        if((!home || !*home) && std::getenv("USERPROFILE")) home = std::getenv("USERPROFILE");
#endif

        if(home && *home) lay.homeRoot = fs::path(home) / ".byeol";
        else              lay.homeRoot = lay.executableDir / ".byeol";

        lay.activeToolchainFile  = lay.homeRoot / "active-toolchain.txt";
        lay.userToolchainsDir    = lay.homeRoot / "toolchains";
        lay.bundledToolchainsDir = _detectBundledToolchainsDir(lay.executableDir);
        lay.bundledWorkerPath    = lay.bundledToolchainsDir / "local" / lay.workerFileName;
        return lay;
    }

    bool me::_isWorkerFile(const fs::path& path) {
        std::error_code ec;
        return fs::is_regular_file(path, ec);
    }

    std::string me::_trim(std::string value) {
        auto isSpace = [](unsigned char ch) { return std::isspace(ch) != 0; };
        value.erase(value.begin(), std::find_if_not(value.begin(), value.end(), isSpace));
        value.erase(std::find_if_not(value.rbegin(), value.rend(), isSpace).base(), value.end());
        return value;
    }

    fs::path me::_detectExecutablePath(const char* argv0) {
#ifdef _WIN32
        char buffer[MAX_PATH] = {0};
        DWORD length = GetModuleFileNameA(nullptr, buffer, MAX_PATH);
        if(length > 0) return fs::path(std::string(buffer, length));
#elif defined(__APPLE__)
        char buffer[PATH_MAX] = {0};
        uint32_t size = sizeof(buffer);
        if(_NSGetExecutablePath(buffer, &size) == 0) {
            char resolved[PATH_MAX] = {0};
            if(realpath(buffer, resolved)) return fs::path(resolved);
        }
#else
        char buffer[PATH_MAX] = {0};
        ssize_t length = ::readlink("/proc/self/exe", buffer, sizeof(buffer));
        if(length > 0) return fs::path(std::string(buffer, static_cast<std::size_t>(length)));
#endif
        if(argv0 && *argv0) return fs::absolute(argv0);
        return fs::current_path() / "byeol";
    }

    fs::path me::_detectBundledToolchainsDir(const fs::path& executableDir) {
        fs::path adjacent = executableDir / "toolchains";
        if(fs::exists(adjacent)) return adjacent;

        fs::path sibling = executableDir.parent_path() / "toolchains";
        if(fs::exists(sibling)) return sibling;

        return adjacent;
    }

    bool me::_isSafeVersionString(const std::string& version) {
        if(version.empty()) return false;
        return std::all_of(version.begin(), version.end(), [](unsigned char c) {
            return std::isalnum(c) || c == '.' || c == '-';
        });
    }

    std::string me::_buildDownloadUrl(const std::string& version) {
        return kGithubReleaseBase + version + "/byeol-" + kOs + "-" + kArch + ".zip";
    }

    bool me::_downloadFile(const std::string& url, const fs::path& destPath) {
        CURL* curl = curl_easy_init();
        if(!curl) return false;

        std::ofstream file(destPath, std::ios::binary);
        if(!file) {
            curl_easy_cleanup(curl);
            return false;
        }

        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1L);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curlWriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &file);
        curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, curlProgressCallback);
        curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);

        CURLcode res = curl_easy_perform(curl);
        std::cout << "\n";
        curl_easy_cleanup(curl);

        if(res != CURLE_OK) {
            std::cerr << "download error: " << curl_easy_strerror(res) << "\n";
            file.close();
            std::error_code ec;
            fs::remove(destPath, ec);
            return false;
        }
        return true;
    }

    bool me::_extractZip(const fs::path& zipPath, const fs::path& destDir) {
        void* reader = mz_zip_reader_create();

        if(mz_zip_reader_open_file(reader, zipPath.string().c_str()) != MZ_OK) {
            std::cerr << "failed to open zip: " << zipPath << "\n";
            mz_zip_reader_delete(&reader);
            return false;
        }

        bool ok    = true;
        int32_t err = mz_zip_reader_goto_first_entry(reader);

        while(err == MZ_OK) {
            if(mz_zip_reader_entry_is_dir(reader) == MZ_OK) {
                err = mz_zip_reader_goto_next_entry(reader);
                continue;
            }

            mz_zip_file* info = nullptr;
            if(mz_zip_reader_entry_get_info(reader, &info) != MZ_OK || !info) {
                std::cerr << "failed to get entry info\n";
                ok = false;
                break;
            }

            fs::path dest = destDir / info->filename;

            // Guard against zip-slip: reject entries that escape destDir via "../".
            const fs::path normalizedRoot = destDir.lexically_normal();
            const fs::path normalizedDest = dest.lexically_normal();
            const auto rel = normalizedDest.lexically_relative(normalizedRoot);
            if(rel.empty() || *rel.begin() == "..") {
                std::cerr << "unsafe zip entry rejected: " << info->filename << "\n";
                ok = false;
                break;
            }

            std::error_code ec;
            fs::create_directories(dest.parent_path(), ec);
            if(ec) {
                std::cerr << "failed to create dir: " << dest.parent_path() << "\n";
                ok = false;
                break;
            }

            if(mz_zip_reader_entry_save_file(reader, dest.string().c_str()) != MZ_OK) {
                std::cerr << "failed to extract: " << info->filename << "\n";
                ok = false;
                break;
            }

#ifndef _WIN32
            uint32_t unixMode = (info->external_fa >> 16) & 0xFFFF;
            if(unixMode != 0) ::chmod(dest.string().c_str(), unixMode & 0777);
#endif

            err = mz_zip_reader_goto_next_entry(reader);
        }

        mz_zip_reader_close(reader);
        mz_zip_reader_delete(&reader);
        return ok;
    }

    void me::_ensureHomeLayout() const {
        std::error_code ignored;
        fs::create_directories(_layout.userToolchainsDir, ignored);
    }

} // namespace by
