#include "launcher/toolchain/program.hpp"

#include <clog/when.hpp>

#include <filesystem>

namespace by {
    BY(DEF_ME(program))

    me::program(const std::string& path): _path(path) {}

    const type& me::getType() const { return ttype<me>::get(); }

    nbool me::isValid() {
        // TODO: OS/arch 검증은 상위 toolchain::isValid()에서 수행.
        //       여기서는 파일 존재만 확인.
        std::error_code ec;
        return std::filesystem::exists(_path, ec) && !ec;
    }

    nint me::run(const args& a) {
        // child process 를 spawn 하고 stdio 를 상속시킨 뒤 종료까지 wait 한다.
        // spawn 실패 시 종료 코드 1 로 반환.
        execArgs execA(a.begin(), a.end());
        WHEN(!_proc.create(_path, execA)).err("failed to spawn worker: %s", _path.c_str()).ret(1);
        return _proc.wait();
    }

    nint me::runInSession(const args& a) {
        // TODO: stdio 인터셉트, DAP/REPL 후크가 들어갈 자리.
        //       지금은 run()과 동일 동작으로 뼈대만 유지한다.
        return run(a);
    }

    const std::string& me::getPath() const {
        return _path;
    }
}
