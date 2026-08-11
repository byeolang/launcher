#pragma once

#include "launcher/common.hpp"
#include "launcher/toolchain/toolable.hpp"

#include <indep/process.hpp>

#include <string>
#include <vector>

namespace by {

    // toolchain의 하나의 프로그램을 나타내는 클래스.
    // indep의 process를 사용해서 임의의 프로그램을 실행하고 결과를 반환한다.
    // program의 위치, 존재여부, 단발성 실행인지 session을 바인딩해서 실행하는지를
    // 담당한다.
    class _nout program : public instance, public toolable {
        BY(ME(program, instance), INIT_META(program), CLONE(me))

    public:
        program(const std::string& path); // path는 절대경로여야 한다.

    public:
        nbool isValid() override;
        nint run(const args& a) override;
        nint runInSession(const args& a) override;

        const type& getType() const override;

        const std::string& getPath() const;

    private:
        std::string _path;   // 이 프로그램의 절대 경로
        process _proc;
    };

    typedef std::vector<program> programs;
}
