#include "launcher/flag/launchStacker.hpp"
#include "launcher/flag/flags/listFlag.hpp"
#include "launcher/flag/flags/proxyFlag.hpp"
#include "launcher/launcher.hpp"

namespace by {
    BY(DEF_ME(launchStacker))

    me::launchStacker(launcher& l): _launcher(l) {}

    void me::_initFlags(flags& tray) const {
        // 각 서브커맨드에 대응하는 flag 를 여기 등록한다.
        // 모든 flag 는 launcher 참조를 받아 launcher 의 메서드만 호출한다.
        //
        // TODO: installFlag, useFlag, runFlag 도 같은 패턴으로 추가.
        //       proxyFlag 는 아직 필수 override(_onTake 등) 가 비어있어 등록 보류.
        tray.push_back(new listFlag(_launcher));
    }
}
