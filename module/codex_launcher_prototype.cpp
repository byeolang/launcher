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
#endif

#include "miniz.h"

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
#  error "unsupported architecture. only x86_64 is supported."
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
        const std::string activeVersion = currentVersion();

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

// TODO: replace with static libcurl once HTTP library is decided
bool downloadFile(const std::string& url, const fs::path& destPath) {
    std::string dest = destPath.string();
    std::string cmd =
        "curl -L --fail --progress-bar"
        " -o \"" + dest + "\""
        " \"" + url + "\"";
    return std::system(cmd.c_str()) == 0;
}

bool extractZip(const fs::path& zipPath, const fs::path& destDir) {
    mz_zip_archive zip{};
    if(!mz_zip_reader_init_file(&zip, zipPath.string().c_str(), 0)) {
        std::cerr << "failed to open zip: "
                  << mz_zip_get_error_string(mz_zip_get_last_error(&zip)) << "\n";
        return false;
    }

    mz_uint numFiles = mz_zip_reader_get_num_files(&zip);
    bool ok = true;

    for(mz_uint i = 0; i < numFiles; ++i) {
        mz_zip_archive_file_stat stat{};
        if(!mz_zip_reader_file_stat(&zip, i, &stat)) continue;
        if(stat.m_is_directory) continue;

        fs::path dest = destDir / stat.m_filename;

        std::error_code ec;
        fs::create_directories(dest.parent_path(), ec);
        if(ec) {
            std::cerr << "failed to create dir: " << dest.parent_path() << "\n";
            ok = false;
            break;
        }

        if(!mz_zip_reader_extract_to_file(&zip, i, dest.string().c_str(), 0)) {
            std::cerr << "failed to extract: " << stat.m_filename
                      << " (" << mz_zip_get_error_string(mz_zip_get_last_error(&zip)) << ")\n";
            ok = false;
            break;
        }

#ifndef _WIN32
        // Restore Unix permissions stored in zip external_attr (upper 16 bits).
        uint32_t unixMode = (stat.m_external_attr >> 16) & 0xFFFF;
        if(unixMode != 0) ::chmod(dest.string().c_str(), unixMode & 0777);
#endif
    }

    mz_zip_reader_end(&zip);
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
