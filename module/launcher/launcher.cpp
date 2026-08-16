#include "launcher/launcher.hpp"
#include "launcher/installer/installer.hpp"

#include <stela/ast/stela.hpp>
#include <stela/ast/valStela.hpp>
#include <stela/ast/verStela.hpp>
#include <stela/parser/stelaParser.hpp>

#include <clog/when.hpp>

#include <filesystem>

namespace by {

    static const std::string TOOLCHAIN_DIR = "toolchain";
    static constexpr const nchar* KEY_USING_VER = "usingVer";
    static constexpr const nchar* CONFIG_FILENAME = "config.stela";

    BY(DEF_ME(launcher))

    ver::ver(nint major, nint minor, nint fix, nbool newIsInstalled, nbool newIsAvailable)
        : super(major, minor, fix), isInstalled(newIsInstalled), isAvailable(newIsAvailable) {}
    ver::ver(const std::string& verStr, nbool newIsInstalled, nbool newIsAvailable)
        : super(verStr), isInstalled(newIsInstalled), isAvailable(newIsAvailable) {}
    ver::ver(const nchar* verStr, nbool newIsInstalled, nbool newIsAvailable)
        : super(verStr), isInstalled(newIsInstalled), isAvailable(newIsAvailable) {}


    // {cwd}/toolchain/<ver>/ 경로.
    namespace {
        static std::string _getToolchainPath(const verStela& ver) {
            return TOOLCHAIN_DIR + "/" + ver.asStr();
        }
    }

    me::launcher() {
        if(!_loadConfig()) return;
        _chain = tstr<toolchain>(new toolchain(_getToolchainPath(getVer())));
    }

    // try to load config.stela file and if not exist, assume that this first app starting.
    // so download latest version and use it.
    nbool me::_loadConfig() {
        _config = stelaParser().parseFromFile(CONFIG_FILENAME);
        WHEN(_config).ret(true);

        auto version = downloadLatest();
        WHEN_NUL(version).err(
            "it seems that this is first app starting so tried to download "
            "latest toolchains but failed to download. please check your network.").ret(false);
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
        WHEN(fsystem::find(_getToolchainPath(ver)).isEnd())
            .err("toolchain not installed; run `install <ver>` first.").ret(false);
        WHEN(!_config).err("%s not loaded; run `use <ver>` first.", CONFIG_FILENAME).ret(false);

        _config->add(new valStela(ver, KEY_USING_VER));
        return stelaWriter().writeFile(*_config, CONFIG_FILENAME);
    }

    tstr<verStela> me::downloadLatest() {
        auto list = getAllVers();
        installer inst;
        return inst.getLatest();
    }

    const type& me::getType() const { return ttype<me>::get(); }

    vers me::_downloadVers() const {
    }

    vers me::getAllVers() const {
        auto versions = _downloadVers();

        auto e = fsystem::find(TOOLCHAIN_DIR + "/*.stela");
        while(e.next()) {
            
        }
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
