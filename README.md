# ProjectNight

Boost.Asio 기반 비동기 TCP Echo Server/Client 프로젝트.

## 사전 요구 사항

| 도구 | 비고 |
|------|------|
| **Visual Studio 2022** | MSVC 컴파일러(`cl.exe`), C++23 지원 필요 |
| **CMake 3.8+** | Visual Studio 설치 시 함께 설치 가능 |
| **Ninja** | Visual Studio 설치 시 함께 설치 가능 |
| **vcpkg** | 아래 설치 방법 참고 |

## vcpkg 설치

CMakePresets.json이 vcpkg를 **프로젝트 상위 디렉터리**에서 참조하므로, 반드시 아래 경로에 클론해야 합니다.

```
repos/
├── vcpkg/            ← 여기에 클론
└── ProjectNight/     ← 이 프로젝트
```

```bat
cd C:\Users\{사용자}\source\repos
git clone https://github.com/microsoft/vcpkg.git
cd vcpkg
bootstrap-vcpkg.bat
```

> 경로가 다를 경우 `NightServer/CMakePresets.json`과 `NightClient/CMakePresets.json`의
> `CMAKE_TOOLCHAIN_FILE` 값을 수정하세요.

## 라이브러리 설치

vcpkg로 **Boost**를 설치합니다. (Boost.Asio, Boost.Lockfree 등은 Boost에 포함되어 있습니다)

```bat
cd C:\Users\{사용자}\source\repos\vcpkg
vcpkg install boost:x64-windows
```

## 코드 컨벤션

| 항목 | 규칙 |
|------|------|
| C++ 표준 | C++23 |
| 네이밍 | 클래스 `PascalCase`, 함수/변수 `snake_case`, 멤버 변수 `snake_case_` (후행 밑줄) |
| 포맷팅 | 4칸 스페이스, Allman 중괄호 |
| 헤더 가드 | `#pragma once` |
| 네임스페이스 | `using namespace std;` 금지, 항상 `std::` 명시 |
| 에러 처리 | `try/catch` 금지, `std::expected` + `boost::system::error_code` 사용 |
| 클래스 순서 | public -> protected -> private |

## 빌드 및 실행

프로젝트 루트에 포함된 bat 파일로 빌드부터 실행까지 한 번에 처리할 수 있습니다.
Visual Studio 환경 변수 설정, CMake Configure, Build를 자동으로 수행합니다.

### 서버 실행

```bat
run_server.bat
```

서버가 **포트 12345**에서 시작됩니다.

### 클라이언트 실행

```bat
:: 클라이언트 1개 실행
run_client.bat

:: 클라이언트 여러 개 실행 (예: 3개)
run_client.bat 3
```

클라이언트 수를 인자로 넘기면 해당 수만큼 별도 창에서 동시에 실행됩니다.

## 테스트 방법

1. `run_server.bat`를 실행하여 서버를 먼저 띄웁니다.
2. 새 터미널(또는 별도 cmd 창)에서 `run_client.bat`를 실행합니다.
3. 클라이언트 창에서 텍스트를 입력하면, 서버가 그대로 에코하여 돌려보냅니다.
4. 여러 클라이언트를 동시에 테스트하려면 `run_client.bat 3`처럼 숫자를 지정합니다.

## 빌드 산출물 경로

빌드 결과물은 각 프로젝트의 `out/build/x64-debug/` 디렉터리에 생성됩니다.

```
NightServer/out/build/x64-debug/NightServer.exe
NightClient/out/build/x64-debug/NightClient.exe
```

## 트러블슈팅

| 증상 | 해결 |
|------|------|
| `Visual Studio를 찾을 수 없습니다` | Visual Studio 2022가 설치되어 있는지 확인 |
| `Configure 실패` | vcpkg 경로가 올바른지, `vcpkg install boost:x64-windows`를 실행했는지 확인 |
| `Build 실패` | C++23을 지원하는 MSVC 버전인지 확인 (VS 2022 17.x 권장) |
| 클라이언트 접속 불가 | 서버가 먼저 실행 중인지 확인, 방화벽에서 포트 12345 허용 여부 확인 |
