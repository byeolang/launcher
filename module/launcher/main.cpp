#include "launcher/launcher.hpp"
#include "launcher/flag/launchStacker.hpp"

using namespace by;

int main(int argc, char* argv[]) {
    flagArgs args;
    for(int n = 1; n < argc; n++)
        args.emplace_back(std::string(argv[n]));

    launcher app;
    WHEN(!app.isValid()).err("").ret(0);
    launchStacker stacker(app);
    flagable::res r = stacker.take(args);
    WHEN(r == flagable::EXIT_PROGRAM).ret(0);

    return app.run(args);
}
