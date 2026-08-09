namespace by {

    // toolchain의 하나의 프로그램을 나타내는 클래스
    // indep의 process를 사용해서 임의의 프로그램을 실행하고 결과를 반환한다.
    // program의 위치, 존재여부, 단발성 실행인지 session을 바인딩해서 실행하는지를
    // 담당한다.
    class _nout program : public toolable {
        program(const std::string& path); // path는 절대경로여야 한다.

        // prgoram의 byeol 파일을 실행하고 결과를 반환. 입력값도 받아야 함.
        // prgoram의 실행이 끝날때까지 대기. 결과를 string으로 반환.
        run();
        // prgoram의 byeol 프로세스를 실행하고 그것과 바인딩한다.
        // 실행 도중에 byeol의 표준출력과 표준에러를 읽을 수 있어야 함.
        runInSession();

        isValid() override;
        doesExist() override;
        install();

        std::string _path; // 이 프로그램의 절대 경로
    };
}
