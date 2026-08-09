namespace by {
    // toolchain과 program의 공통 인터페이스
    class _nout toolable {
        nbool isValid(); // 이 객체가 valid한지 확인.
        nbool doesExist(); // 이 객체가 바인딩하는 program이 존재하는지 확인.
    };
}
