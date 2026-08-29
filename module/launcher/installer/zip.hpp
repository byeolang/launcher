#pragma once

#include "launcher/common.hpp"

// mz_zip_rw.h names types from mz_zip.h and mz_strm.h in its prototypes but
// doesn't include either, so both have to come first.
#include <mz.h>
#include <mz_strm.h>
#include <mz_zip.h>
#include <mz_zip_rw.h>

#include <string>

namespace by {

    /**
     * @brief RAII wrapper for a single minizip-ng reader handle
     * @details owns the handle, so a caller only picks which archive to open and
     *          where its content should land.
     * @code
     *  zip z;
     *  zip::res done = z.unzip(archivePath, destPath);
     *  WHEN(!done.has()).err("unzip failed: %d", done.getErr()).ret(false);
     * @endcode
     */
    class _nout zip {
        BY(ME(zip))

    public:
        /**
         * @brief what 1 extraction gave back
         * @details carries where the content landed when it worked and the reason it
         *          stopped when it didn't, so a failure can't outlive the call that
         *          made it. the error is one of the MZ_* codes in mz.h.
         */
        typedef tres<std::string, nint> res;

    public:
        zip();
        ~zip();

        /** @brief a reader handle has a single owner, so it can't be duplicated. */
        zip(const me& rhs) = delete;
        me& operator=(const me& rhs) = delete;

    public:
        /** @brief whether the handle was acquired. every extraction fails when this is false. */
        nbool isValid() const;

        /**
         * @brief extracts every entry of the archive at archivePath under destPath
         * @details used for a toolchain zip. entry names are resolved against
         *          destPath, so an entry can't escape it. what was already written
         *          stays on a failure: destPath is the caller's to remove, unlike the
         *          single file curl::downloadAsFile() owns.
         * @return destPath, or the MZ_* code it stopped with.
         */
        res unzip(const std::string& archivePath, const std::string& destPath);

    private:
        void _rel();

    private:
        void* _handle;
    };
}
