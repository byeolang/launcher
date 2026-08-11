#include "launcher/launcher.hpp"
#include "launcher/flag/launchStacker.hpp"

using namespace by;

int main(int argc, char* argv[]) {
    // launcher 는 config.stela 를 읽어 초기화된다. config 가 없어도 크래시하지 않고,
    // install/use/list 서브커맨드는 계속 동작한다. run 계열만 _chain 이 있어야 성공.
    launcher app;

    // argv 를 flagArgs 로 옮겨 launchStacker 에 흘려보낸다.
    // 각 flag 는 app 의 메서드를 호출해 실제 도메인 로직을 수행한다.
    flagArgs a;
    for(int n = 1; n < argc; n++)
        a.emplace_back(argv[n]);

    launchStacker stacker(app);
    flagable::res r = stacker.take(a);

    // 매칭된 flag 가 EXIT_PROGRAM 을 반환했다면 그 flag 가 종료 흐름을 담당.
    // 아니면 남은 args 를 launcher.run 으로 passthrough.
    if(r == flagable::EXIT_PROGRAM) return 0;

    args passthrough(a.begin(), a.end());
    return app.run(passthrough);
}
