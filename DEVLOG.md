# ProjectNight 개발 로그

## 공통 설정

### 1. vcpkg 연동 설정

- vcpkg 경로: `C:\Users\NHN\source\repos\vcpkg`
- 각 프로젝트의 `CMakePresets.json`에서 `windows-base` preset에 toolchain 설정:
  ```json
  "CMAKE_TOOLCHAIN_FILE": "${sourceDir}/../../vcpkg/scripts/buildsystems/vcpkg.cmake"
  ```
- 모든 preset(x64-debug, x64-release, x86-debug, x86-release)이 `windows-base`를 상속하므로 자동 적용됨

#### vcpkg로 새 라이브러리 추가하는 법
```powershell
# 1) 설치
C:\Users\NHN\source\repos\vcpkg\vcpkg install <패키지명>

# 2) CMakeLists.txt에 추가
find_package(<패키지> REQUIRED CONFIG)
target_link_libraries(<프로젝트명> PRIVATE <타겟>::<타겟>)
```

---

### 2. Boost.Asio 추가

- 설치: `vcpkg install boost-asio`
- CMakeLists.txt (NightServer, NightClient 공통):
  ```cmake
  find_package(Boost REQUIRED CONFIG)
  target_link_libraries(<프로젝트명> PRIVATE Boost::headers)
  ```
- Boost.Asio는 **헤더 전용** 라이브러리이므로 `Boost::headers` 타겟을 사용

---

### 3. C++ 표준을 20 → 23으로 변경

- `CMakeLists.txt`에서 `CXX_STANDARD 23` 으로 설정
- 이를 통해 `std::expected`, `std::print`, `std::flat_map` 등 C++23 기능 사용 가능
- 참고: `CXX_STANDARD 23`은 CMake 3.20+ 필요 (현재 3.31 사용 중이라 문제 없음)

---

### 4. std::expected 기반 에러 처리 패턴

기존 `try/catch`를 모두 제거하고 `std::expected`를 반환하는 함수 구조를 사용.

```cpp
// boost::system::error_code로 에러를 받고
// 실패 시 std::unexpected("에러 메시지")를 반환
auto result = some_function();
if (!result)
{
    std::cerr << result.error() << std::endl;
    break;
}
// 성공 시 result.value()로 값 사용
```

---

### 5. UTF-8 한글 출력 설정

Windows 콘솔 기본 코드페이지(CP949)에서 UTF-8 한글이 깨지는 문제를 해결.

- **컴파일 시**: MSVC `/utf-8` 옵션으로 소스/실행 문자셋을 UTF-8로 통일
  ```cmake
  target_compile_options(<프로젝트명> PRIVATE /utf-8)
  ```
- **실행 시**: bat 파일에서 `chcp 65001`로 콘솔 코드페이지를 UTF-8로 설정
- `<Windows.h>`의 `SetConsoleOutputCP()` 대신 위 조합을 사용하여 불필요한 Windows 헤더 의존을 제거

---

## NightServer (TCP Echo 서버)

### 핵심 함수들

| 함수 | 반환 타입 | 역할 |
|------|-----------|------|
| `read_from()` | `std::expected<string, string>` | 클라이언트에서 데이터 읽기 |
| `write_to()` | `std::expected<void, string>` | 클라이언트에 데이터 보내기 |
| `start_server()` | `std::expected<tcp::acceptor, string>` | 서버 소켓 열기/바인드/리슨 |
| `accept_client()` | `std::expected<tcp::socket, string>` | 클라이언트 접속 수락 |

### 서버 테스트 방법

서버 실행 후 (포트 12345):
```powershell
telnet localhost 12345
```
또는 Python:
```python
import socket
s = socket.socket()
s.connect(("localhost", 12345))
s.send(b"Hello!\n")
print(s.recv(1024))
s.close()
```

---

## NightClient (TCP Echo 클라이언트)

NightServer와 동일한 `std::expected` 에러 처리 패턴을 적용한 Echo 클라이언트.

### 핵심 함수들

| 함수 | 반환 타입 | 역할 |
|------|-----------|------|
| `connect_to_server()` | `std::expected<tcp::socket, string>` | 서버에 TCP 연결 |
| `send_to()` | `std::expected<void, string>` | 서버에 데이터 전송 |
| `read_from()` | `std::expected<string, string>` | 서버로부터 데이터 수신 |

### 동작 흐름

1. `connect_to_server()`로 localhost:12345에 연결
2. 표준 입력(`std::cin`)으로 메시지를 받아 서버에 전송
3. 서버로부터 에코 응답을 받아 출력
4. `Ctrl+Z`로 종료

---

## 현재 프로젝트 구조

```
ProjectNight/
├── DEVLOG.md                    # 이 파일
├── run_server.bat               # 서버 빌드 & 실행 스크립트
├── run_client.bat               # 클라이언트 빌드 & 실행 스크립트
├── NightServer/
│   ├── CMakeLists.txt           # 빌드 설정 (Boost, C++23, /utf-8)
│   ├── CMakePresets.json        # vcpkg toolchain 연동
│   ├── NightServer.h            # 헤더 (Boost.Asio, std::expected)
│   └── NightServer.cpp          # TCP Echo 서버 본체
└── NightClient/
    ├── CMakeLists.txt           # 빌드 설정 (Boost, C++23, /utf-8)
    ├── CMakePresets.json        # vcpkg toolchain 연동
    ├── NightClient.h            # 헤더 (Boost.Asio, std::expected)
    └── NightClient.cpp          # TCP Echo 클라이언트 본체
```
