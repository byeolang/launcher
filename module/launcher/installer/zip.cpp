#include "launcher/installer/zip.hpp"

#include <filesystem>

namespace by {

    BY(DEF_ME(zip))

    me::zip(): _handle(nullptr) {
        // a ctor can't use the WHEN macro. isValid() reports a failed handle instead.
        _handle = mz_zip_reader_create();
    }

    me::~zip() { _rel(); }

    nbool me::isValid() const { return _handle != nullptr; }

    me::res me::unzip(const std::string& archivePath, const std::string& destPath) {
        WHEN(!isValid()).err("zip handle isn't ready.").ret(res(MZ_MEM_ERROR));

        // minizip only creates the directories its entry names ask for, so the root
        // they hang off has to be there first.
        std::error_code ec;
        std::filesystem::create_directories(destPath, ec);
        WHEN(ec && !std::filesystem::is_directory(destPath))
            .err("failed to make %s to extract into.", destPath.c_str())
            .ret(res(MZ_OPEN_ERROR));

        nint err = mz_zip_reader_open_file(_handle, archivePath.c_str());
        WHEN(err != MZ_OK).err("failed to open %s: %d", archivePath.c_str(), err).ret(res(err));

        err = mz_zip_reader_save_all(_handle, destPath.c_str());
        // an archive holding no entry stops on the first walk, which leaves destPath
        // empty rather than short. only a cut walk turns MZ_END_OF_LIST into a failure.
        if(err == MZ_END_OF_LIST) err = MZ_OK;

        // closing can still fail, but nothing it reports changes what already landed
        // on disk, so the extraction result stands.
        mz_zip_reader_close(_handle);
        WHEN(err != MZ_OK).err("failed to extract %s: %d", archivePath.c_str(), err).ret(res(err));
        return res(destPath);
    }

    void me::_rel() {
        WHEN(!isValid()).ret();

        mz_zip_reader_delete(&_handle);
        _handle = nullptr;
    }
}
