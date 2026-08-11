#pragma once

#include "launcher/common.hpp"

#include <stela/ast/verStela.hpp>

#include <string>
#include <vector>

namespace by {

    typedef std::vector<verStela> vers;

    // 다운로드 stub 결과.
    // TODO: 실제 downloader가 생기면 여기서 파일 스트림/에러 상세를 담게 확장.
    struct downloadRes {
        nbool ok = false;
        std::string data;
    };

    // toolchain master manifest로부터 어떠한 toolchain이 이 OS에 available한지를 판단한다.
    // network에 연결되어있다면 toolchain master manifest를 github에 배포된 url
    // 로부터 받아온다. 없다면 로컬에 있는 master manifest를 그냥 사용한다.
    class _nout installer {
        BY(ME(installer))

    public:
        // downloader를 통해서 network에 연결되어있다면 toolchain version list를
        // github에 배포된 url 로부터 받아온다. 네트워크가 없다면 로컬에서 가져온다.
        // 만약 네트워크로 다운로드를 못했는데 로컬에도 version list가 없다면
        // 에러로써 빈 vers를 내보낸다.
        vers getList();

        nbool install(const verStela& ver) const;

    private:
        downloadRes _download(const std::string& url) const;

        // miniz를 사용해서 압축을 푼다.
        // 정상적으로 풀어졌는지 확인한다. 에러없으면 true
        nbool _unzip(const std::string& path) const;
    };
}
