// Reference-only launcher prototype.
// This file is intentionally self-contained so the structure is easy to copy or rewrite.

#include <algorithm>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <set>
#include <stdexcept>
#include <string>
#include <system_error>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#else
#include <cerrno>
#include <cstring>
#include <limits.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif
#endif

#include "mz.h"
#include "mz_strm.h"
#include "mz_zip.h"
#include "mz_zip_rw.h"
#include <curl/curl.h>

namespace fs = std::filesystem;

// === Platform detection ===
// These drive the download URL and worker binary name.
#if defined(_WIN32)
static constexpr const char* kOs = "win";
static constexpr const char* kArch = "x64";
static constexpr const char* kWorkerExeName = "byeol.exe";
#elif defined(__APPLE__)
static constexpr const char* kOs = "macos";
#if defined(__arm64__) || defined(__aarch64__)
static constexpr const char* kArch = "arm64";
#else
static constexpr const char* kArch = "x64";
#endif
static constexpr const char* kWorkerExeName = "byeol";
#else // Linux / Termux
static constexpr const char* kOs = "ubuntu";
#if defined(__x86_64__)
static constexpr const char* kArch = "x64";
#else
static constexpr const char* kArch = "x64"; // TODO: restore #error after feasibility test
#endif
static constexpr const char* kWorkerExeName = "byeol";
#endif

static const std::string kGithubReleaseBase =
    "https://github.com/byeolang/byeol/releases/download/v";

namespace {

enum class LaunchMode {
    OneShot,
    SessionBound,
};

struct LaunchRequest {
    std::string command;
    std::string subcommand;
    LaunchMode mode = LaunchMode::OneShot;
    std::string explicitToolchain;
    std::vector<std::string> passthroughArgs;
};

struct ToolchainEntry {
    std::string version;
    fs::path workerPath;
    bool active = false;
    bool bundled = false;
};

struct ToolchainLayout {
    fs::path executablePath;
    fs::path executableDir;
    fs::path homeRoot;
    fs::path activeToolchainFile;
    fs::path userToolchainsDir;
    fs::path bundledToolchainsDir;
    fs::path bundledWorkerPath;
    std::string workerFileName;
};

std::string trim(std::string value) {
    auto isSpace = [](unsigned char ch) { return std::isspace(ch) != 0; };
    value.erase(value.begin(), std::find_if_not(value.begin(), value.end(), isSpace));
    value.erase(std::find_if_not(value.rbegin(), value.rend(), isSpace).base(), value.end());
    return value;
}

fs::path detectExecutablePath(const char* argv0) {
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

fs::path detectBundledToolchainsDir(const fs::path& executableDir) {
    const fs::path adjacent = executableDir / "toolchains";
    if(fs::exists(adjacent)) return adjacent;

    const fs::path sibling = executableDir.parent_path() / "toolchains";
    if(fs::exists(sibling)) return sibling;

    return adjacent;
}

ToolchainLayout makeLayout(const char* argv0) {
    ToolchainLayout layout;
    layout.executablePath = detectExecutablePath(argv0);
    layout.executableDir = layout.executablePath.parent_path();
    layout.workerFileName = kWorkerExeName;

    const char* home = std::getenv("HOME");
#ifdef _WIN32
    if((!home || !*home) && std::getenv("USERPROFILE")) home = std::getenv("USERPROFILE");
#endif

    if(home && *home) layout.homeRoot = fs::path(home) / ".byeol";
    else layout.homeRoot = layout.executableDir / ".byeol";

    layout.activeToolchainFile = layout.homeRoot / "active-toolchain.txt";
    layout.userToolchainsDir = layout.homeRoot / "toolchains";
    layout.bundledToolchainsDir = detectBundledToolchainsDir(layout.executableDir);
    layout.bundledWorkerPath = layout.bundledToolchainsDir / "local" / layout.workerFileName;
    return layout;
}

class ToolchainStore {
public:
    explicit ToolchainStore(ToolchainLayout layout): _layout(std::move(layout)) {}

    const ToolchainLayout& layout() const { return _layout; }

    std::string currentVersion() const {
        std::ifstream input(_layout.activeToolchainFile);
        if(input) {
            std::string version;
            std::getline(input, version);
            version = trim(version);
            if(!version.empty()) return version;
        }

        if(fs::exists(_layout.bundledWorkerPath)) return "local";

        auto installed = list();
        if(!installed.empty()) return installed.front().version;
        return {};
    }

    fs::path resolveWorkerPath(const std::string& version) const {
        if(version.empty()) return {};

        if(version == "local" && fs::exists(_layout.bundledWorkerPath)) {
            return _layout.bundledWorkerPath;
        }

        fs::path bundled = _layout.bundledToolchainsDir / version / _layout.workerFileName;
        if(fs::exists(bundled)) return bundled;

        fs::path userManaged = _layout.userToolchainsDir / version / _layout.workerFileName;
        if(fs::exists(userManaged)) return userManaged;

        return {};
    }

    bool use(const std::string& version) {
        fs::path workerPath = resolveWorkerPath(version);
        if(workerPath.empty()) return false;

        ensureHomeLayout();
        std::ofstream output(_layout.activeToolchainFile, std::ios::trunc);
        if(!output) throw std::runtime_error("failed to write active toolchain metadata");
        output << version << "\n";
        return true;
    }

    std::vector<ToolchainEntry> list() const {
        std::vector<ToolchainEntry> entries;
        std::set<std::string> seen;

        // read active version directly to avoid mutual recursion with currentVersion()
        std::string activeVersion;
        {
            std::ifstream input(_layout.activeToolchainFile);
            if(input) {
                std::getline(input, activeVersion);
                activeVersion = trim(activeVersion);
            }
        }
        if(activeVersion.empty() && fs::exists(_layout.bundledWorkerPath))
            activeVersion = "local";

        auto addEntry = [&](const std::string& version, const fs::path& workerPath, bool bundled) {
            if(version.empty() || workerPath.empty()) return;
            if(!seen.insert(version).second) return;
            entries.push_back(ToolchainEntry{version, workerPath, version == activeVersion, bundled});
        };

        if(fs::exists(_layout.bundledToolchainsDir)) {
            for(const auto& entry: fs::directory_iterator(_layout.bundledToolchainsDir)) {
                if(!entry.is_directory()) continue;
                fs::path worker = entry.path() / _layout.workerFileName;
                if(!fs::exists(worker)) continue;
                addEntry(entry.path().filename().string(), worker, true);
            }
        }

        if(fs::exists(_layout.userToolchainsDir)) {
            for(const auto& entry: fs::directory_iterator(_layout.userToolchainsDir)) {
                if(!entry.is_directory()) continue;
                fs::path worker = entry.path() / _layout.workerFileName;
                if(!fs::exists(worker)) continue;
                addEntry(entry.path().filename().string(), worker, false);
            }
        }

        std::sort(entries.begin(), entries.end(), [](const ToolchainEntry& lhs, const ToolchainEntry& rhs) {
            return lhs.version < rhs.version;
        });
        return entries;
    }

private:
    void ensureHomeLayout() const {
        std::error_code ignored;
        fs::create_directories(_layout.userToolchainsDir, ignored);
    }

private:
    ToolchainLayout _layout;
};

// ===== Download & Install =====

bool isSafeVersionString(const std::string& version) {
    if(version.empty()) return false;
    return std::all_of(version.begin(), version.end(), [](unsigned char c) {
        return std::isalnum(c) || c == '.' || c == '-';
    });
}

std::string buildDownloadUrl(const std::string& version) {
    return kGithubReleaseBase + version
        + "/byeol-" + kOs + "-" + kArch + ".zip";
}

static std::size_t curlWriteCallback(char* ptr, std::size_t size, std::size_t nmemb, void* userdata) {
    auto* out = static_cast<std::ofstream*>(userdata);
    out->write(ptr, static_cast<std::streamsize>(size * nmemb));
    if(!*out) return 0;  // 0 반환 시 curl이 CURLE_WRITE_ERROR로 중단
    return size * nmemb;
}

static int curlProgressCallback(void* /*clientp*/, curl_off_t dltotal, curl_off_t dlnow,
                                  curl_off_t /*ultotal*/, curl_off_t /*ulnow*/) {
    if(dltotal <= 0) return 0;
    int pct = static_cast<int>(dlnow * 100 / dltotal);
    std::cout << "\r  " << pct << "% (" << dlnow / 1024 << " / " << dltotal / 1024 << " KB)   "
              << std::flush;
    return 0;
}

bool downloadFile(const std::string& url, const fs::path& destPath) {
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
        file.close();  // must close before remove on Windows
        std::error_code ec;
        fs::remove(destPath, ec);
        return false;
    }
    return true;
}

bool extractZip(const fs::path& zipPath, const fs::path& destDir) {
    void* reader = mz_zip_reader_create();

    if(mz_zip_reader_open_file(reader, zipPath.string().c_str()) != MZ_OK) {
        std::cerr << "failed to open zip: " << zipPath << "\n";
        mz_zip_reader_delete(&reader);
        return false;
    }

    bool ok = true;
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

int handleInstall(const std::string& version, ToolchainStore& toolchains) {
    if(!isSafeVersionString(version)) {
        std::cerr << "invalid version: " << version << "\n";
        return 1;
    }

    if(!toolchains.resolveWorkerPath(version).empty()) {
        std::cout << "toolchain " << version << " is already installed.\n";
        return 0;
    }

    const std::string url = buildDownloadUrl(version);
    std::cout << "platform: " << kOs << "-" << kArch << "\n";
    std::cout << "url:      " << url << "\n";

    const fs::path tmpZip =
        fs::temp_directory_path() / ("byeol-" + version + "-portable.zip");

    std::cout << "downloading...\n";
    if(!downloadFile(url, tmpZip)) {
        std::cerr << "download failed. check version or platform availability.\n";
        return 1;
    }

    const fs::path installDir = toolchains.layout().bundledToolchainsDir / version;
    std::cout << "extracting to: " << installDir.string() << "\n";

    std::error_code ec;
    fs::create_directories(installDir, ec);

    const bool extracted = extractZip(tmpZip, installDir);
    fs::remove(tmpZip, ec);

    if(!extracted) {
        std::cerr << "extraction failed.\n";
        fs::remove_all(installDir, ec);
        return 1;
    }

    const fs::path workerPath = installDir / kWorkerExeName;
    if(!fs::exists(workerPath)) {
        std::cerr << "worker not found after extraction: " << workerPath << "\n";
        return 1;
    }

    std::cout << "installed: " << workerPath.string() << "\n";
    return 0;
}

// ===== Argument parsing =====

LaunchRequest parseRunInvocation(const std::vector<std::string>& args, std::size_t startIndex) {
    LaunchRequest request;
    request.command = "run";
    request.subcommand = "execute";

    std::size_t index = startIndex;
    while(index < args.size()) {
        const std::string& token = args[index];

        if(token == "--") {
            ++index;
            break;
        }

        if(token == "--session") {
            request.mode = LaunchMode::SessionBound;
            ++index;
            continue;
        }

        if(token == "--toolchain") {
            if(index + 1 >= args.size()) throw std::runtime_error("missing value after --toolchain");
            request.explicitToolchain = args[index + 1];
            index += 2;
            continue;
        }

        break;
    }

    request.passthroughArgs.assign(args.begin() + static_cast<std::ptrdiff_t>(index), args.end());
    return request;
}

LaunchRequest parseArgs(int argc, char** argv) {
    std::vector<std::string> args;
    for(int i = 1; i < argc; ++i)
        args.emplace_back(argv[i]);

    if(args.empty()) return LaunchRequest{"help", "", LaunchMode::OneShot, "", {}};

    if(args[0] == "-h" || args[0] == "--help" || args[0] == "help") {
        return LaunchRequest{"help", "", LaunchMode::OneShot, "", {}};
    }

    if(args[0] == "--version" || args[0] == "version") {
        return LaunchRequest{"version", "", LaunchMode::OneShot, "", {}};
    }

    if(args[0] == "toolchain") {
        LaunchRequest request;
        request.command = "toolchain";
        if(args.size() >= 2) request.subcommand = args[1];
        if(args.size() > 2) {
            request.passthroughArgs.assign(args.begin() + 2, args.end());
            if((request.subcommand == "use" || request.subcommand == "install")
               && !request.passthroughArgs.empty()) {
                request.explicitToolchain = request.passthroughArgs.front();
            }
        }
        return request;
    }

    if(args[0] == "self" && args.size() >= 2 && args[1] == "update") {
        return LaunchRequest{"self", "update", LaunchMode::OneShot, "", {}};
    }

    if(args[0] == "run") return parseRunInvocation(args, 1);

    return parseRunInvocation(args, 0);
}

// ===== Output helpers =====

void printHelp(const ToolchainLayout& layout) {
    std::cout
        << "byeol launcher prototype\n"
        << "\n"
        << "Command tree:\n"
        << "  byeol <script-or-args...>\n"
        << "  byeol run <script-or-args...>\n"
        << "  byeol run --session <script-or-args...>\n"
        << "  byeol run --toolchain <version> <script-or-args...>\n"
        << "  byeol toolchain list\n"
        << "  byeol toolchain current\n"
        << "  byeol toolchain install <version>\n"
        << "  byeol toolchain use <version>\n"
        << "  byeol self update\n"
        << "\n"
        << "Platform:    " << kOs << "-" << kArch << "\n"
        << "Worker name: " << kWorkerExeName << "\n"
        << "\n"
        << "Bundled worker path:\n"
        << "  " << layout.bundledWorkerPath.string() << "\n"
        << "\n"
        << "Active toolchain metadata:\n"
        << "  " << layout.activeToolchainFile.string() << "\n";
}

void printVersion() {
    std::cout << "byeol launcher prototype 0.1.0\n";
}

// ===== Process spawn/wait =====

struct ChildProcess {
#ifdef _WIN32
    PROCESS_INFORMATION processInfo{};
    bool running = false;
#else
    pid_t pid = -1;
#endif
};

#ifdef _WIN32
std::string joinArgs(const std::vector<std::string>& values) {
    auto quote = [](const std::string& v) {
        if(v.find_first_of(" \t\"") == std::string::npos) return v;
        std::string out = "\"";
        for(char ch: v) {
            if(ch == '"') out += "\\\"";
            else out += ch;
        }
        return out + "\"";
    };

    std::string out;
    for(std::size_t i = 0; i < values.size(); ++i) {
        if(i > 0) out += " ";
        out += quote(values[i]);
    }
    return out;
}

ChildProcess spawnWorker(const fs::path& workerPath, const std::vector<std::string>& passthroughArgs) {
    std::vector<std::string> allArgs;
    allArgs.push_back(workerPath.string());
    allArgs.insert(allArgs.end(), passthroughArgs.begin(), passthroughArgs.end());

    std::string cmdLine = joinArgs(allArgs);
    std::vector<char> mutableCmd(cmdLine.begin(), cmdLine.end());
    mutableCmd.push_back('\0');

    STARTUPINFOA si{};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi{};

    if(!CreateProcessA(nullptr, mutableCmd.data(), nullptr, nullptr,
                       TRUE, 0, nullptr, nullptr, &si, &pi)) {
        throw std::runtime_error("failed to launch worker process");
    }

    ChildProcess child;
    child.processInfo = pi;
    child.running = true;
    return child;
}
#else
ChildProcess spawnWorker(const fs::path& workerPath, const std::vector<std::string>& passthroughArgs) {
    pid_t pid = ::fork();
    if(pid < 0)
        throw std::runtime_error(std::string("fork() failed: ") + std::strerror(errno));

    if(pid == 0) {
        std::vector<std::string> ownedArgs;
        ownedArgs.push_back(workerPath.string());
        ownedArgs.insert(ownedArgs.end(), passthroughArgs.begin(), passthroughArgs.end());

        std::vector<char*> argv;
        argv.reserve(ownedArgs.size() + 1);
        for(std::string& a: ownedArgs) argv.push_back(a.data());
        argv.push_back(nullptr);

        ::execv(workerPath.c_str(), argv.data());
        std::cerr << "failed to exec worker: " << workerPath << " (" << std::strerror(errno) << ")\n";
        ::_exit(127);
    }

    return ChildProcess{pid};
}
#endif

int waitForWorker(ChildProcess& child) {
#ifdef _WIN32
    WaitForSingleObject(child.processInfo.hProcess, INFINITE);
    DWORD exitCode = 1;
    GetExitCodeProcess(child.processInfo.hProcess, &exitCode);
    CloseHandle(child.processInfo.hThread);
    CloseHandle(child.processInfo.hProcess);
    child.running = false;
    return static_cast<int>(exitCode);
#else
    int status = 0;
    while(::waitpid(child.pid, &status, 0) < 0) {
        if(errno == EINTR) continue;
        throw std::runtime_error(std::string("waitpid() failed: ") + std::strerror(errno));
    }

    if(WIFEXITED(status)) return WEXITSTATUS(status);
    if(WIFSIGNALED(status)) return 128 + WTERMSIG(status);
    return 1;
#endif
}

int dispatchOneShot(const fs::path& workerPath, const std::vector<std::string>& passthroughArgs) {
    ChildProcess child = spawnWorker(workerPath, passthroughArgs);
    return waitForWorker(child);
}

int dispatchSessionBound(const fs::path& workerPath, const std::vector<std::string>& passthroughArgs) {
    ChildProcess child = spawnWorker(workerPath, passthroughArgs);
    // Placeholder for future session wiring: DAP / REPL / watch / LSP.
    return waitForWorker(child);
}

// ===== Command handlers =====

int handleToolchainCommand(const LaunchRequest& request, ToolchainStore& toolchains) {
    if(request.subcommand == "install") {
        if(request.explicitToolchain.empty()) {
            std::cerr << "usage: byeol toolchain install <version>\n";
            return 1;
        }
        return handleInstall(request.explicitToolchain, toolchains);
    }

    if(request.subcommand == "list") {
        auto entries = toolchains.list();
        if(entries.empty()) {
            std::cout << "no installed toolchains\n";
            return 0;
        }

        for(const auto& entry: entries) {
            std::cout << (entry.active ? "* " : "  ") << entry.version
                      << (entry.bundled ? " (bundled)" : " (user)") << " -> "
                      << entry.workerPath.string() << "\n";
        }
        return 0;
    }

    if(request.subcommand == "current") {
        const std::string version = toolchains.currentVersion();
        if(version.empty()) {
            std::cerr << "no active toolchain\n";
            return 1;
        }

        fs::path workerPath = toolchains.resolveWorkerPath(version);
        std::cout << version;
        if(!workerPath.empty()) std::cout << " -> " << workerPath.string();
        std::cout << "\n";
        return workerPath.empty() ? 1 : 0;
    }

    if(request.subcommand == "use") {
        if(request.explicitToolchain.empty()) {
            std::cerr << "usage: byeol toolchain use <version>\n";
            return 1;
        }

        if(!toolchains.use(request.explicitToolchain)) {
            std::cerr << "toolchain not found: " << request.explicitToolchain << "\n";
            std::cerr << "hint: byeol toolchain install " << request.explicitToolchain << "\n";
            return 1;
        }

        std::cout << "active toolchain set to " << request.explicitToolchain << "\n";
        return 0;
    }

    std::cerr << "usage: byeol toolchain <list|current|install|use>\n";
    return 1;
}

int handleRunCommand(const LaunchRequest& request, ToolchainStore& toolchains) {
    const std::string version =
        request.explicitToolchain.empty() ? toolchains.currentVersion() : request.explicitToolchain;

    if(version.empty()) {
        std::cerr << "no active toolchain and no explicit toolchain provided\n";
        std::cerr << "hint: byeol toolchain install <version>\n";
        return 1;
    }

    const fs::path workerPath = toolchains.resolveWorkerPath(version);
    if(workerPath.empty()) {
        std::cerr << "worker not found for toolchain: " << version << "\n";
        std::cerr << "hint: byeol toolchain install " << version << "\n";
        return 1;
    }

    if(request.mode == LaunchMode::SessionBound)
        return dispatchSessionBound(workerPath, request.passthroughArgs);

    return dispatchOneShot(workerPath, request.passthroughArgs);
}

int handleSelfUpdate() {
    std::cout
        << "self update is a placeholder in this prototype\n"
        << "TODO: download launcher artifact, swap binary, preserve active toolchain metadata\n";
    return 0;
}

int runLauncher(int argc, char** argv) {
    ToolchainStore toolchains(makeLayout(argc > 0 ? argv[0] : nullptr));
    LaunchRequest request = parseArgs(argc, argv);

    if(request.command == "help") {
        printHelp(toolchains.layout());
        return 0;
    }

    if(request.command == "version") {
        printVersion();
        return 0;
    }

    if(request.command == "toolchain") return handleToolchainCommand(request, toolchains);
    if(request.command == "self" && request.subcommand == "update") return handleSelfUpdate();
    if(request.command == "run") return handleRunCommand(request, toolchains);

    std::cerr << "unknown command: " << request.command << "\n";
    return 1;
}

} // namespace

int main(int argc, char** argv) {
    try {
        return runLauncher(argc, argv);
    } catch(const std::exception& ex) {
        std::cerr << "launcher error: " << ex.what() << "\n";
        return 1;
    } catch(...) {
        std::cerr << "launcher error: unknown exception\n";
        return 1;
    }
}
