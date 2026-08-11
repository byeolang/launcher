#include "launcher/flag/flags/listFlag.hpp"
#include "launcher/launcher.hpp"

#include <stela/ast/verStela.hpp>

#include <iostream>

namespace by {
    BY(DEF_ME(listFlag))

    me::listFlag(launcher& l): _launcher(l) {}

    const nchar* me::getName() const { return "list"; }

    const nchar* me::getDescription() const {
        return R"DESC(
    show installed and downloadable toolchain versions.)DESC";
    }

    const strings& me::_getRegExpr() const {
        static strings inner{"^list$"};
        return inner;
    }

    me::res me::_onTake(const flagArgs&) const {
        listResult r = _launcher.list();

        std::cout << "installed:\n";
        if(r.installed.empty()) std::cout << "  (none)\n";
        for(const verStela& v: r.installed)
            std::cout << "  " << v.asStr() << "\n";

        std::cout << "available:\n";
        if(r.available.empty()) std::cout << "  (none)\n";
        for(const verStela& v: r.available)
            std::cout << "  " << v.asStr() << "\n";

        return EXIT_PROGRAM;
    }
}
