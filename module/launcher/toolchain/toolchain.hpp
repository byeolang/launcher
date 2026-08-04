#pragma once

#include "launcher/common.hpp"
#include <filesystem>
#include <string>
#include <vector>

namespace by {

    class _nout toolchain {
        BY(ME(toolchain))

    public:
        struct entry {
            std::string           version;
            std::filesystem::path workerPath;
            bool                  active  = false;
            bool                  bundled = false;
        };

        struct layout {
            std::filesystem::path executablePath;
            std::filesystem::path executableDir;
            std::filesystem::path homeRoot;
            std::filesystem::path activeToolchainFile;
            std::filesystem::path userToolchainsDir;
            std::filesystem::path bundledToolchainsDir;
            std::filesystem::path bundledWorkerPath;
            std::string           workerFileName;
        };

    public:
        explicit toolchain(layout lay);

        const layout&         getLayout() const;
        std::string           currentVersion() const;
        std::filesystem::path resolveWorkerPath(const std::string& version) const;
        bool                  use(const std::string& version);
        std::vector<entry>    list() const;
        int                   install(const std::string& version);

        static layout makeLayout(const char* argv0);

    private:
        static bool                  _isWorkerFile(const std::filesystem::path& path);
        static std::string           _trim(std::string value);
        static std::filesystem::path _detectExecutablePath(const char* argv0);
        static std::filesystem::path _detectBundledToolchainsDir(const std::filesystem::path& executableDir);
        static bool                  _isSafeVersionString(const std::string& version);
        static std::string           _buildDownloadUrl(const std::string& version);
        static bool                  _downloadFile(const std::string& url, const std::filesystem::path& destPath);
        static bool                  _extractZip(const std::filesystem::path& zipPath, const std::filesystem::path& destDir);
        void                         _ensureHomeLayout() const;

    private:
        layout _layout;
    };
}
