# ProjectNight

Boost.Asio 기반 비동기 TCP 채팅 서버/클라이언트 프로젝트.

## 프로젝트 구조

| 디렉터리 | 설명 |
|----------|------|
| [**NightNetwork**](NightNetwork/README.md) | TCP 네트워크 정적 라이브러리 (구조, API, 사용 예시는 링크 참조) |
| [**schema**](schema/README.md) | FlatBuffers 기반 채팅 프로토콜 스키마 및 메시지 설명 |
| **NightServer** | NightNetwork를 사용하는 채팅 서버 애플리케이션 |
| **NightClient** | NightNetwork를 사용하는 채팅 클라이언트 애플리케이션 |

## 주요 기능

- TCP 기반 비동기 채팅 서버/클라이언트
- 표시 이름(login) 요청 및 중복 이름 검사
- 채팅방 목록 조회 및 방 입장
- 방 사용자 입장/퇴장 이벤트 브로드캐스트
- 같은 방 사용자 대상 채팅 메시지 브로드캐스트
- 기본 채팅방 예시 제공: `Lobby`, `Dev`, `Chat`

## 사전 요구 사항

| 도구 | 비고 |
|------|------|
| **Visual Studio 2022** | MSVC 컴파일러(`cl.exe`), C++23 지원 필요 |
| **CMake 3.25+** | Visual Studio 설치 시 함께 설치 가능 |
| **Ninja** | Visual Studio 설치 시 함께 설치 가능 |
| **Git** | 서브모듈 clone에 필요 |

## 프로젝트 받기

vcpkg가 Git 서브모듈로 포함되어 있으므로, `--recursive` 옵션으로 clone합니다.

```bat
git clone --recursive <repo-url>
cd ProjectNight
```

이미 clone한 경우 서브모듈만 따로 받을 수 있습니다.

```bat
git submodule update --init --recursive
```

## vcpkg 부트스트랩

처음 한 번만 실행하면 됩니다.

```bat
vcpkg\bootstrap-vcpkg.bat
```

> 라이브러리는 별도로 설치할 필요 없습니다.
> CMake configure 시 `vcpkg.json` 매니페스트를 기반으로 자동 설치됩니다.

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

프로젝트 루트의 `scripts/` 폴더에 포함된 bat 파일로 빌드부터 실행까지 한 번에 처리할 수 있습니다.
Visual Studio 환경 변수 설정, CMake Configure, Build를 자동으로 수행합니다.

### 서버 실행

```bat
scripts\run_server.bat
```

서버가 **포트 12345**에서 시작됩니다.

### 클라이언트 실행

```bat
:: 클라이언트 1개 실행
scripts\run_client.bat

:: 클라이언트 여러 개 실행 (예: 3개)
scripts\run_client.bat 3
```

클라이언트 수를 인자로 넘기면 해당 수만큼 별도 창에서 동시에 실행됩니다.

## 프로토콜 문서

채팅 프로토콜 구조와 메시지 타입 설명은 [`schema/README.md`](schema/README.md) 문서를 참조합니다.

## 테스트 방법

1. `scripts/run_server.bat`를 실행하여 채팅 서버를 먼저 띄웁니다.
2. 새 터미널(또는 별도 cmd 창)에서 `scripts/run_client.bat`를 실행합니다.
3. 클라이언트에서 표시 이름을 입력하고 서버에 접속합니다.
4. 방 목록을 새로고침한 뒤 원하는 채팅방에 입장합니다.
5. 메시지를 입력하면 같은 방에 있는 다른 클라이언트들에게 브로드캐스트됩니다.
6. 여러 클라이언트를 동시에 테스트하려면 `scripts/run_client.bat 3`처럼 숫자를 지정합니다.

## 수동 빌드

bat 파일 없이 직접 빌드할 수도 있습니다.

```bat
cmake --preset x64-debug
cmake --build out/build/x64-debug
```

특정 타겟만 빌드하려면:

```bat
cmake --build out/build/x64-debug --target NightServer
cmake --build out/build/x64-debug --target NightClient
```

## 빌드 산출물 경로

빌드 결과물은 루트의 `out/build/x64-debug/` 하위에 생성됩니다.

```
out/build/x64-debug/NightServer/NightServer.exe
out/build/x64-debug/NightClient/NightClient.exe
```

## 트러블슈팅

| 증상 | 해결 |
|------|------|
| `Visual Studio를 찾을 수 없습니다` | Visual Studio 2022가 설치되어 있는지 확인 |
| `Configure 실패` | `vcpkg\bootstrap-vcpkg.bat`을 실행했는지 확인, 서브모듈이 제대로 받아졌는지 `git submodule status`로 확인 |
| `Build 실패` | C++23을 지원하는 MSVC 버전인지 확인 (VS 2022 17.x 권장) |
| 클라이언트 접속 불가 | 서버가 먼저 실행 중인지 확인, 방화벽에서 포트 12345 허용 여부 확인 |
