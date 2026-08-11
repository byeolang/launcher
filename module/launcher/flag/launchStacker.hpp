#pragma once

#include "launcher/common.hpp"

#include <flagStacker/stacker.hpp>

namespace by {

    class launcher;

    // launcher 프로세스의 argv 를 파싱하고 매칭된 flag 의 동작을 실행한다.
    // 모든 flag 는 launcher 참조를 받아 launcher 의 메서드만 호출한다.
    class _nout launchStacker : public stacker {
        BY(ME(launchStacker, stacker))

    public:
        launchStacker(launcher& l);

    protected:
        void _initFlags(flags& tray) const override;

    private:
        launcher& _launcher;
    };
}
