#pragma once

#include "launcher/common.hpp"

namespace by {
    class _nout proxyFlag : public flag {
        BY(ME(proxyFlag, flag))

    public:
        const nchar* getName() const override;
    };
}
