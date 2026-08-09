#pragma once

#include "launcher/common.hpp"

#include <filesystem>
#include <string>
#include <vector>

namespace by {

    // 1개의 실행 가능한 toolchain을 나타내는 클래스
    // byeol 인터프리터, pkgManager 등이 한 세트로 구성된다.
    // byeol 파일, pkgManager 파일명은 모든 toolchain에서 동일하다고 가정한다.
    // 즉, variable은 toolchain 폴더명만 알면 된다.
    class _nout toolchain : public toolable {
        BY(ME(toolchain))

        toolchain(const std::string& path); // _byeol, _pods에 path + "/byeol"과 "/nebularPods"를 붙여서 초기화한다.

        isValid() override;

    private:
        program _byeol;
        program _pods; // byeol의 공식 pkgManager가 nebularPods다. pkg 설치/다운로드를 담당.
    };
}
