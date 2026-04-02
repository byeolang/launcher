# Codex 작업 프롬프트 — byeol launcher 프로토타입 설계 및 구현

너는 byeol 프로젝트의 C++/CMake 코드베이스에서 **launcher(frontend) 프로토타입**을 구현하는 엔지니어다.

이번 작업의 목적은, 사용자에게는 `byeol` 하나만 보이되 내부적으로는 적절한 worker를 선택해 실행하는 **launcher/toolchain manager**의 첫 프로토타입을 만드는 것이다.

중요: 이 작업은 **프로토타이핑**이다. 지나치게 거대한 아키텍처를 새로 만들지 말고, 현재 코드베이스에 자연스럽게 들어갈 수 있는 수준으로 구현하라.

---

## 0. 이미 확정된 전제

- 최신 `flagStacker`는 이미 별도 GitHub repo로 분리되어 있고, 현재 byeol 프로젝트는 이것을 외부에서 `FetchContent`로 가져와 사용하는 상태다.
- 따라서 예전 frontend의 `interpreter`, `starter` 직접 의존 문제를 다시 걱정하지 말고, **현재 flagStacker는 launcher용 입력 해석 계층으로 충분히 쓸 수 있다**고 가정하라.
- 이번 작업에서 너는 `flagStacker` 자체를 재설계하지 않는다.
- 사용자는 최종적으로 **`byeol` 하나만 보아야 하며**, 내부 worker exe는 직접 노출하지 않는 방향이다.
- launcher/frontend는 **toolchain manager 역할**도 맡는다.
- frontend는 기본적으로 아래 2가지 실행 모드를 가져야 한다.
  - `one-shot mode`
  - `session-bound mode`
- 이 구조는 나중에 `DAP`, `REPL`, `LSP`, `watch mode` 등으로 확장 가능해야 한다.

---

## 1. 구현 목표

이번 작업의 목표는 아래를 만족하는 **최소 기능 launcher 프로토타입**이다.

### 1-1. launcher의 역할

launcher(`byeol`)는 다음 3가지 역할을 가진다.

1. **명령 해석기**
   - 사용자의 argv를 해석한다.
2. **모드 선택기**
   - 실행 모드가 `one-shot`인지 `session-bound`인지 결정한다.
3. **worker 디스패처**
   - 현재 활성화된 toolchain/version에 맞는 worker 경로를 선택하고 실행한다.

### 1-2. 이번 프로토타입에서 지원할 명령

아래 명령 트리를 기본으로 구현하라.

- `byeol <script-or-args...>`
  - 기본 run 명령으로 간주
  - 기본 mode는 `one-shot`
- `byeol run <script-or-args...>`
- `byeol run --session <script-or-args...>`
- `byeol toolchain list`
- `byeol toolchain current`
- `byeol toolchain use <version>`
- `byeol self update`

설명:
- `toolchain`과 `self update`는 launcher가 직접 처리한다.
- `run` 계열은 launcher가 worker로 넘긴다.
- 기본 naked invocation(`byeol foo.by`)도 내부적으로 `run`으로 normalize 해도 된다.

---

## 2. 설계 원칙

### 2-1. command와 mode를 분리하라

플래그 처리와 별도로, 내부 모델에서 아래 개념은 분리되어야 한다.

- `command`
- `subcommand`
- `launchMode`
- `explicitToolchain`
- `passthroughArgs`

예시:

```cpp
struct launchRequest {
    std::string command;
    std::string subcommand;
    launchMode mode;
    std::string explicitToolchain;
    std::vector<std::string> passthroughArgs;
};
```

`launchMode`는 최소 아래 enum 정도면 충분하다.

```cpp
enum class launchMode {
    oneShot,
    sessionBound
};
```

당장은 두 개만 구현하되, 나중에 다른 모드가 추가되어도 자연스럽게 switch/case 또는 dispatcher 확장이 가능하도록 구조를 잡아라.

### 2-2. launcher는 실행 본체가 아니다

launcher는 parser/verifier/runtime을 직접 실행하는 본체가 아니다.

- 실제 언어 실행은 worker(`byeol-exec` 같은 내부 실행기)가 담당
- launcher는 **고르고, 넘기고, 기다리고, 상태를 관리**하는 앞문 역할

즉, launcher는 "똑똑한 진입점"이어야 한다.

### 2-3. 프로토타입이므로 과도한 IPC/DAP 구현은 금지

이번 작업에서는 아래는 **설계 여지만 열고 실제 구현은 최소화**하라.

- DAP 프로토콜 구현
- REPL 구현
- LSP 구현
- watch mode 구현
- updater helper 실제 구현
- bindgen/pack worker 세분화

이번 단계에서는 `session-bound mode`가 들어갈 자리와 실행 흐름의 뼈대만 보이면 충분하다.

---

## 3. toolchain 관리 프로토타입 요구사항

### 3-1. active toolchain metadata

launcher는 현재 활성 toolchain/version을 읽을 수 있어야 한다.

예를 들어 내부적으로 아래와 같은 개념을 가져도 된다.

- active toolchain metadata file
- versioned worker directory

예시 개념:

```text
~/.byeol/
  active-toolchain.json
  toolchains/
    0.9.0/
      byeol-exec
    0.9.1/
      byeol-exec
```

실제 경로는 코드베이스와 플랫폼 제약에 맞춰 조정 가능하다.

### 3-2. 최소 기능

- `toolchain list`
  - 설치된 toolchain 목록을 보여준다.
- `toolchain current`
  - 현재 active toolchain을 보여준다.
- `toolchain use <version>`
  - active toolchain metadata를 바꾼다.

이번 단계에서는 설치/다운로드 기능까지 다 구현하지 않아도 된다.
다만 launcher가 **현재 active version을 읽고**, 그에 해당하는 worker를 선택하는 흐름은 실제 코드로 보이게 하라.

---

## 4. run / dispatch 프로토타입 요구사항

### 4-1. one-shot mode

`one-shot`은 아래 흐름이면 된다.

1. active toolchain 결정
2. worker 경로 결정
3. worker 실행
4. argv passthrough
5. worker 종료코드 반환
6. launcher 종료

### 4-2. session-bound mode

`session-bound`는 아래 흐름의 뼈대만 보여주면 된다.

1. active toolchain 결정
2. worker 경로 결정
3. worker 실행
4. launcher가 연결/세션 생존 상태 유지
5. worker 종료 시 launcher 종료

실제 복잡한 세션 프로토콜은 생략 가능하다.
다만 구조상 `oneShot`과 다른 경로로 분기되는 코드가 있어야 한다.

---

## 5. 파일/코드 작업 지시

### 5-1. 우선 할 일

현재 코드베이스를 탐색해서 아래를 파악하라.

- 기존 frontend main 진입점
- 현재 CLI 처리 구조
- `flagStacker`가 연결된 지점
- 현재 실행기(main executable)와 라이브러리 구조
- worker 후보(`byeol-exec`, 기존 byeol 실행기 역할)와의 접점

### 5-2. 구현 방향

아래 순서로 작업하라.

1. launcher용 request 모델 추가
2. command/subcommand/mode 정규화 로직 추가
3. toolchain metadata 읽기/쓰기 최소 유틸 추가
4. worker path resolution 함수 추가
5. one-shot dispatch 구현
6. session-bound dispatch 뼈대 구현
7. `toolchain list/current/use` 구현
8. `self update`는 placeholder 명령으로라도 entrypoint 추가
9. 도움말/오류 메시지를 최소한으로 정리

### 5-3. 코드 스타일

- 현재 byeol 코드 스타일과 naming을 최대한 존중
- 지나치게 현대 C++ 전용 문법으로 튀지 말 것
- 기존 코드가 가진 구조와 어휘를 우선 따를 것
- 필요 없는 리팩토링 금지
- 이번 작업과 무관한 대규모 파일 이동 금지

---

## 6. 산출물 요구사항

Codex는 최종 응답에서 아래를 반드시 포함하라.

1. **무엇을 구현했는지 요약**
2. **왜 그렇게 설계했는지 짧은 설명**
3. **수정한 파일 목록**
4. **핵심 코드 포인트 설명**
5. **남겨둔 TODO / 다음 단계**

가능하면 아래도 포함하라.

- launcher command tree 요약
- active toolchain metadata 예시
- one-shot / session-bound 분기 구조 요약

---

## 7. 금지사항

- `flagStacker`를 다시 설계하거나 갈아엎지 말 것
- DAP/LSP/watch mode를 실제 완성하려고 들지 말 것
- updater 구현을 과도하게 깊게 들어가지 말 것
- 실제 core/runtime 구조를 크게 깨뜨리는 리팩토링 금지
- 사용자가 보게 될 명령어 체계를 불필요하게 복잡하게 만들지 말 것

---

## 8. 구현 완료 기준

아래가 보이면 프로토타입 성공으로 본다.

- `byeol run ...` 이 내부적으로 worker 실행으로 이어지는 구조가 코드상 드러남
- `byeol run --session ...` 이 별도 mode 분기로 들어감
- `byeol toolchain list/current/use` 가 동작하거나 최소한 실제 metadata를 건드리는 수준으로 구현됨
- `byeol self update` entrypoint가 존재함
- launcher가 command / mode / worker selection 을 분리해 다루는 구조가 드러남
- 나중에 mode가 추가되어도 자연스럽게 확장 가능한 코드 형태임

---

## 9. 참고 컨텍스트 요약

프로젝트 문서 기준으로 현재 방향은 다음과 같다.

- 사용자에게는 `byeol` 하나만 보이게 한다.
- 내부적으로는 toolchain/frontend가 versioned worker를 관리한다.
- frontend는 `one-shot mode`와 `session(bound) mode` 두 가지 기본 모드를 가진다.
- 이 구조는 나중에 DAP/REPL/LSP/watch mode로 확장되기 좋다.
- `toolchain list/use/current`, `self update` 는 frontend 레이어가 맡는다.
- 내부 worker exe는 직접 노출하지 않는다.

이 방향을 벗어나지 않는 선에서 프로토타입을 구현하라.

---

## 10. 최종 요청

위 요구사항을 만족하도록 **실제 코드 수정안**을 제시하라.
가능하면 바로 적용 가능한 수준의 patch를 만들어라.
불명확한 부분은 과도하게 질문하지 말고, 현재 코드베이스에 가장 자연스럽게 들어가는 보수적인 선택을 하라.
