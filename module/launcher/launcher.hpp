#pragma once

#include "launcher/common.hpp"
#include "launcher/toolchain/toolable.hpp"
#include "launcher/toolchain/toolchain.hpp"

#include <stela/ast/verStela.hpp>

#include <vector>

namespace by {

    class stela;
    typedef std::vector<verStela> vers;

    // launcher.list() 결과. 설치된 버전과 다운로드 가능한 버전을 함께 담는다.
    struct _nout listResult {
        vers installed;   // {cwd}/toolchain/<ver>/ 에 실제 존재하는 버전들
        vers available;   // installer 를 통해 원격에서 받아올 수 있는 버전들
    };

    // launcher는 하나의 프로세스에서 하나의 toolchain 만 가리킨다.
    // 사용자가 `use` 로 활성 버전을 바꾸거나 `install` 로 새 버전을 받는 것은
    // 모두 별도의 one-shot 명령이며, 그 이후 프로세스는 종료된다.
    //
    // 도메인 로직(설치/전환/조회)은 모두 launcher 에 있다.
    // 각 flag 는 여기 정의된 메서드를 호출만 하는 얇은 껍데기다.
    class _nout launcher : public instance, public toolable {
        BY(ME(launcher, instance), CLONE(me), INIT_META(launcher))

    public:
        // cwd 의 config.stela 를 읽어 usingVer 를 얻고,
        // {cwd}/toolchain/<usingVer>/ 로 toolchain 을 구성한다.
        // config.stela 가 없거나 usingVer 를 못 찾으면 _chain 은 null 상태.
        //   → run/runInSession 은 실패하지만, install/use/list 는 정상 동작.
        launcher();

    public:
        nbool isValid() override;

        // 그대로 _chain->run()으로 위임한다. _chain이 없으면 에러.
        nint run(const args& a) override;

        // 그대로 _chain->runInSession()으로 위임한다. _chain이 없으면 에러.
        nint runInSession(const args& a) override;

        // 현재 활성 toolchain 의 버전. _chain 이 없으면 dummy(0.0.0).
        const verStela& getVer() const;

        // 지역 installer 를 만들어 ver 을 설치한다.
        // 성공/실패만 반환하며 상태(_chain)는 건드리지 않는다.
        // (설치 후에도 별도 `use` 를 사용자가 명시적으로 호출해야 한다.)
        nbool install(const verStela& ver);

        // {cwd}/toolchain/<ver>/ 존재 여부를 확인하고, 있으면 cwd 의 config.stela 에서
        // `usingVer` 를 ver 로 바꾼 뒤 파일에 저장한다.
        // 폴더가 없으면 사용자가 install 을 먼저 하도록 에러 반환.
        nbool use(const verStela& ver);

        // 설치된 목록: {cwd}/toolchain/ 서브디렉토리 스캔.
        // 다운로드가능 목록: installer 에게 조회.
        listResult list() const;

        const type& getType() const override;

    private:
        // cwd 의 config.stela 를 파싱한 결과. usingVer 필드가 여기 들어있다.
        // use() 로 값이 바뀌면 이걸 통해 다시 파일로 저장한다.
        // TODO: stela serialize API 미존재. 지금은 use() 저장 로직이 stub.
        tstr<stela> _config;

        // 활성 toolchain. usingVer 가 없거나 해당 폴더가 없으면 null.
        tstr<toolchain> _chain;
    };
}
