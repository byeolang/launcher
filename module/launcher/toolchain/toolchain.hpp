#pragma once

#include "launcher/common.hpp"
#include "launcher/toolchain/program.hpp"
#include "launcher/toolchain/toolable.hpp"

#include <string>
#include <vector>

namespace by {

    class stela;
    class verStela;

    // 로컬 설치된 1개의 실행 가능한 toolchain을 나타내는 클래스.
    // byeol 인터프리터, pkgManager 등이 한 세트로 구성된다.
    // 즉, variable은 toolchain 폴더명만 알면 된다.
    //
    // manifest는 다음과 같은 포맷이다.
    // ```stela
    //  ver := 0.1.2
    //  os := "linux"
    //  def program1
    //      path := "yourProgram1"
    //  def program2
    //      path := "yourProgramExecutable"
    // ```
    class _nout toolchain : public instance, public toolable {
        BY(ME(toolchain, instance), CLONE(me), INIT_META(toolchain))

    public:
        // toolchain의 작업디렉토리.
        // 이 작업디렉토리를 기반으로 manifest를 읽고,
        // manifest를 기반으로 byeol, pods 등의 프로그램을 바인딩하는 program 객체를
        // 생성해서 _progs에 넣어둔다.
        // 그때 workingDir + "/byeol"과 "/nebularPods"와 같이 초기화한다.
        toolchain(const std::string& workingDir);

    public:
        // 현재 바인딩하는 프로그램이 1개 이상 존재하며, 각 Program 객체들이 모두
        // isValid()를 true로 반환하는지 체크한다.
        // 추가로 manifest의 os가 현재 플랫폼과 일치하는지도 확인한다.
        nbool isValid() override;

        // args[0]으로 program을 찾고, args[1..]을 splice해서 해당 program에 인자로 전달한다.
        nint run(const args& a) override;

        // 동작은 run과 동일. program.runInSession()을 호출하는 것 뿐이다.
        nint runInSession(const args& a) override;

        // _manifest의 `ver`으로부터 생성된 verStela를 가져온다.
        // _manifest에 ver가 없는 경우 "0.0.0" 을 갖는 static verStela dummy를 대신 반환한다.
        const verStela& getVer() const;

        // getVer()와 동작이 같다. `ver` 대신에 `os` 를 찾는 것 뿐이다.
        const std::string& getOs() const;

        const std::string& getWorkingDir() const;

        const type& getType() const override;

    private:
        // 이 toolchain의 루트 디렉토리.
        // manifest 경로, program path resolution의 base가 된다.
        std::string _workingDir;

        // manifest를 파싱한 결과. `ver`, `os`, `def <program>` 노드들을 담는다.
        tstr<stela> _manifest;

        // toolchain에 포함된 프로그램들.
        // toolchain 버전에 따라서 프로그램 구성이 변경될 수 있기 때문에 배열로 받는다.
        programs _progs;
    };

    typedef std::vector<tstr<toolchain>> toolchainList;
}
