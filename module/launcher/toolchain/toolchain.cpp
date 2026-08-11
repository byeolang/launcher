#include "launcher/toolchain/toolchain.hpp"

#include <stela/ast/stela.hpp>
#include <stela/ast/verStela.hpp>
#include <stela/parser/stelaParser.hpp>

#include <clog/when.hpp>

#include <filesystem>

namespace by {

    // 현재 빌드 플랫폼 문자열. manifest 의 `os` 필드와 비교하기 위한 상수.
    // TODO: arch/ABI 는 이번 draft 에서 미검증. 나중에 platform.hpp 로 추출.
#ifdef BY_BUILD_PLATFORM_IS_WINDOWS
    static constexpr const nchar* CURRENT_OS = "windows";
#elif defined(__APPLE__)
    static constexpr const nchar* CURRENT_OS = "macos";
#else
    static constexpr const nchar* CURRENT_OS = "linux";
#endif

    // manifest 파일명. TODO: layout 클래스로 추출 예정.
    static constexpr const nchar* MANIFEST_NAME = "manifest.stela";

    BY(DEF_ME(toolchain))

    me::toolchain(const std::string& workingDir): _workingDir(workingDir) {
        // 생성자는 WHEN 매크로를 쓸 수 없으므로 plain if 로 조기 종료한다.
        const std::string manifestPath = workingDir + "/" + MANIFEST_NAME;

        std::error_code ec;
        if(!std::filesystem::exists(manifestPath, ec) || ec) return;

        _manifest = stelaParser().parseFromFile(manifestPath);
        if(!_manifest) return;

        // manifest 의 top-level 자식들을 순회.
        // `ver`, `os` 는 스칼라 값. `path` 를 가진 노드는 program 정의로 해석한다.
        for(auto it = _manifest->begin(); it != _manifest->end(); ++it) {
            if(!it->second) continue;
            stela& node = *it->second;

            if(!node.has("path")) continue;

            const std::string& relPath = node["path"].asStr();
            if(relPath.empty()) continue;

            _progs.emplace_back(program(workingDir + "/" + relPath));
        }
    }

    nbool me::isValid() {
        WHEN(_progs.empty()).ret(false);
        for(program& p: _progs)
            WHEN(!p.isValid()).ret(false);

        const std::string& os = getOs();
        WHEN(os.empty()).ret(false);
        return os == CURRENT_OS;
    }

    nint me::run(const args& a) {
        // args[0] 으로 program 매칭. args[1..] 는 program 에 그대로 전달.
        WHEN(a.empty()).err("toolchain::run needs args[0] as program name.").ret(1);

        const std::string& progName = a[0];
        args passthrough(a.begin() + 1, a.end());

        for(program& p: _progs) {
            const std::string filename = std::filesystem::path(p.getPath()).filename().string();
            if(filename == progName) return p.run(passthrough);
        }
        return 1;
    }

    nint me::runInSession(const args& a) {
        WHEN(a.empty()).err("toolchain::runInSession needs args[0] as program name.").ret(1);

        const std::string& progName = a[0];
        args passthrough(a.begin() + 1, a.end());

        for(program& p: _progs) {
            const std::string filename = std::filesystem::path(p.getPath()).filename().string();
            if(filename == progName) return p.runInSession(passthrough);
        }
        return 1;
    }

    const verStela& me::getVer() const {
        static const verStela DUMMY(0, 0, 0);
        WHEN_NUL(_manifest).ret(DUMMY);

        const stela& node = _manifest->sub("ver");
        WHEN(!node).ret(DUMMY);

        const verStela* casted = node.cast<verStela>();
        WHEN_NUL(casted).ret(DUMMY);
        return *casted;
    }

    const std::string& me::getOs() const {
        static const std::string DUMMY;
        WHEN_NUL(_manifest).ret(DUMMY);

        const stela& node = _manifest->sub("os");
        WHEN(!node).ret(DUMMY);
        return node.asStr();
    }

    const std::string& me::getWorkingDir() const {
        return _workingDir;
    }

    const type& me::getType() const { return ttype<me>::get(); }
}
