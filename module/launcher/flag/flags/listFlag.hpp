#pragma once

#include "launcher/common.hpp"

#include <flagStacker/flag/flag.hpp>

namespace by {

    class launcher;

    // `byeol list` 서브커맨드.
    // launcher.list() 를 호출해 설치된/다운로드가능 버전을 함께 출력한다.
    // 도메인 로직은 launcher 에 있고, 이 flag 는 단순히 호출/출력만 담당한다.
    class _nout listFlag : public flag {
        BY(ME(listFlag, flag))

    public:
        listFlag(launcher& l);

    public:
        const nchar* getName() const override;
        const nchar* getDescription() const override;

    protected:
        const strings& _getRegExpr() const override;
        res _onTake(const flagArgs& tray) const override;

    private:
        launcher& _launcher;
    };
}
