#include "launcher/installer/installer.hpp"

#include "launcher/installer/curl.hpp"

namespace by {
    BY(DEF_ME(installer))

    vers me::getList() {
        // TODO: master manifest URL/local 파일 로드 후 vers 구성.
        return vers();
    }

    nbool me::install(const verStela& ver) const {
        // TODO: 실제 다운로드/압축해제 흐름.
        //       사용자 방침: installer 내부에 여러 downloader 를 두고 상황에 맞는 것을 선택.
        //       지금은 stub. 항상 실패로 반환하여 호출부의 실패 경로를 검증할 수 있게 한다.
        (void)ver;
        return false;
    }

    downloadRes me::_download(const std::string& url) const {
        // TODO: 파일로 받아야 하는 toolchain zip은 curl::download()로 따로 태운다.
        //       지금은 manifest처럼 메모리로 받는 경우만 다룬다.
        curl session;
        downloadRes res;
        res.ok = session.get(url, res.data);
        return res;
    }

    nbool me::_unzip(const std::string& path) const {
        // TODO: miniz 로 압축 해제.
        (void)path;
        return false;
    }
}
