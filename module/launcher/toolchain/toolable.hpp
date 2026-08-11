#pragma once

#include "launcher/common.hpp"

#include <string>
#include <vector>

namespace by {

    typedef std::vector<std::string> args;

    // toolchain과 program의 공통 인터페이스
    class _nout toolable {
    public:
        virtual ~toolable() {}

        // 이 객체에 문제는 없는지, 현재 바인딩이 유효한지, 파일은 진짜 존재하는지 등을 체크한다.
        virtual nbool isValid() = 0;

        // child process를 생성하고 stdio를 상속시킨 spawn + wait로 구현하고
        // 자식의 result code를 그대로 반환한다.
        // indep의 process를 사용해야 한다.
        virtual nint run(const args& a) = 0;

        // program의 byeol 프로세스를 실행하고 그것과 바인딩한다.
        // 실행 도중에 byeol의 표준출력과 표준에러를 읽을 수 있어야 함.
        // 나중에 작업한다. 일단은 body를 비울것.
        virtual nint runInSession(const args& a) = 0;
    };
}
