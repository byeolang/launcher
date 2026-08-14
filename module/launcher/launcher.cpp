#include "launcher/launcher.hpp"
#include "launcher/installer/installer.hpp"

#include <stela/ast/stela.hpp>
#include <stela/ast/valStela.hpp>
#include <stela/ast/verStela.hpp>
#include <stela/parser/stelaParser.hpp>

#include <clog/when.hpp>

#include <filesystem>

namespace by {

    static constexpr const nchar* TOOLCHAIN_DIR = "toolchain";
    static constexpr const nchar* KEY_USING_VER = "usingVer";

    BY(DEF_ME(launcher))

    ver::ver(nint major, nint minor, nint fix, nbool newIsInstalled, nbool newIsAvailable)
        : super(major, minor, fix), isInstalled(newIsInstalled), isAvailable(newIsAvailable) {}
    ver::ver(const std::string& verStr, nbool newIsInstalled, nbool newIsAvailable)
        : super(verStr), isInstalled(newIsInstalled), isAvailable(newIsAvailable) {}
    ver::ver(const nchar* verStr, nbool newIsInstalled, nbool newIsAvailable)
        : super(verStr), isInstalled(newIsInstalled), isAvailable(newIsAvailable) {}


    // {cwd}/toolchain/<ver>/ 경로.
    static std::string _toolchainPath(const verStela& ver) {
        return std::string(TOOLCHAIN_DIR) + "/" + ver.asStr();
    }

    me::launcher() {
        if(!_loadConfig()) return;

        // 1. cwd 의 config.stela 를 파싱.
        _config = stelaParser().parseFromFile(CONFIG_NAME);
        if(!_config) return;

        // 2. usingVer 로부터 활성 버전을 얻는다.
        const stela& node = _config->sub(KEY_USING_VER);
        if(!node) return;

        const std::string& verStr = node.asStr();
        if(verStr.empty()) return;

        verStela using_(verStr);

        // 3. 해당 toolchain 폴더에서 toolchain 을 구성.
        const std::string dir = _toolchainPath(using_);
        std::error_code ec;
        if(!std::filesystem::exists(dir, ec) || ec) return;

        _chain = tstr<toolchain>(new toolchain(dir));
    }

    nbool me::_loadConfig() {
        _config = stelaParser().parseFromFile("config.stela");
        WHEN(_config).ret(true);

        auto version = downloadLatest();
        WHEN_NUL(version).ret(false);
        return use(*version); // writes config file at this time.
    }

    nbool me::isValid() {
        WHEN_NUL(_chain).ret(false);
        return _chain->isValid();
    }

    nint me::run(const args& a) {
        WHEN_NUL(_chain).err("no active toolchain; run `install <ver>` then `use <ver>` first.").ret(1);
        return _chain->run(a);
    }

    nint me::runInSession(const args& a) {
        WHEN_NUL(_chain).err("no active toolchain; run `install <ver>` then `use <ver>` first.").ret(1);
        return _chain->runInSession(a);
    }

    const verStela& me::getVer() const {
        static const verStela inner(0, 0, 0);
        WHEN_NUL(_chain).ret(inner);
        return _chain->getVer();
    }

    nbool me::install(const verStela& ver) {
        // installer 는 지역 변수. 설치 후 상태는 유지하지 않는다.
        installer inst;
        return inst.install(ver);
    }

    nbool me::use(const verStela& ver) {
        // 1. {cwd}/toolchain/<ver>/ 존재 확인.
        const std::string dir = _toolchainPath(ver);
        std::error_code ec;
        WHEN(!std::filesystem::exists(dir, ec) || ec)
            .err("toolchain not installed; run `install <ver>` first.").ret(false);

        // 2. config.stela 가 없으면 새 stela 를 만든다.
        if(!_config) _config = tstr<stela>(new stela());

        // 3. usingVer 필드를 교체.
        _config->del(KEY_USING_VER);
        _config->add(new valStela(ver.asStr(), KEY_USING_VER));

        // 4. 파일로 저장.
        //    TODO: stela serialize API 미존재. 준비되면 여기서 config.stela 로 다시 쓴다.
        //          plain text 로 usingVer 라인만 쓰는 fallback 은 config 확장 시 깨지므로 피한다.
        return true;
    }

    const type& me::getType() const { return ttype<me>::get(); }

    listResult me::list() const {
        listResult r;

        // 1. 설치된 목록: {cwd}/toolchain/* 서브디렉토리 열거.
        //    파일 단위 탐색용인 fsystem::find 대신 std::filesystem 을 사용 (디렉토리 열거).
        std::error_code ec;
        if(std::filesystem::exists(TOOLCHAIN_DIR, ec) && !ec) {
            for(auto& e: std::filesystem::directory_iterator(TOOLCHAIN_DIR, ec)) {
                if(ec) break;
                if(!e.is_directory()) continue;
                r.installed.emplace_back(verStela(e.path().filename().string()));
            }
        }

        // 2. 다운로드 가능 목록: installer 에게 조회.
        installer inst;
        r.available = inst.getList();
        return r;
    }
}
