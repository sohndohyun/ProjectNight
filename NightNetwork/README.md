# NightNetwork

Asio 기반 비동기 TCP 네트워크 라이브러리.
바이트 운반만 담당하며, 메시지 타입 구분이나 직렬화는 상위 레이어(게임 로직)에 위임한다.

## 특징

- **Asio 캡슐화** — pimpl 패턴으로 public 헤더에서 Boost.Asio 의존성을 완전 제거
- **싱글스레드 API, 멀티스레드 I/O** — public API는 게임 스레드 하나에서 호출, 내부적으로 멀티스레드 I/O 처리
- **Lock-free 큐** — I/O 스레드와 게임 스레드 사이를 `boost::lockfree` 큐로 연결하여 mutex 없이 동작
- **자동 하트비트** — 전송 계층 내부에서 keepalive를 처리하여 게임 레이어에 노출하지 않음
- **메모리 풀** — thread-local LIFO 버퍼 풀로 반복적인 힙 할당을 절감

## 디렉터리 구조

```
NightNetwork/
├── CMakeLists.txt
├── include/
│   └── NightNetwork/
│       ├── Server.h          # Server 공개 API
│       ├── Client.h          # Client 공개 API
│       └── Packet.h          # PacketType, Packet 구조체
└── src/
    ├── Server.cpp            # Server::Impl 구현
    ├── Client.cpp            # Client::Impl 구현
    ├── Session.h             # 개별 TCP 연결 관리 (내부 전용)
    ├── Session.cpp           # Session 구현
    ├── Protocol.h            # 와이어 포맷 상수, Header 구조체
    ├── TransportDetail.h     # 프레임 빌드/디코딩 유틸리티
    └── BufferPool.h          # Thread-local 메모리 풀
```

### Public vs Internal

| 구분 | 파일 | 설명 |
|------|------|------|
| **Public** | `include/NightNetwork/Server.h` | 서버 생성, 패킷 수신, 세션별 전송/브로드캐스트/연결 종료 |
| **Public** | `include/NightNetwork/Client.h` | 서버 연결, 패킷 수신, 전송, 연결 상태 확인 |
| **Public** | `include/NightNetwork/Packet.h` | `PacketType` enum, `Packet` 구조체 |
| **Internal** | `src/Session.h/.cpp` | 개별 TCP 연결의 읽기/쓰기/하트비트 처리 |
| **Internal** | `src/Protocol.h` | 와이어 포맷 상수 (magic, version, 헤더 크기 등) |
| **Internal** | `src/TransportDetail.h` | 헤더 인코딩/디코딩, 프레임 빌드 헬퍼 |
| **Internal** | `src/BufferPool.h` | Thread-local LIFO 버퍼 풀 |

## 아키텍처

```
┌─────────────────────────────────────────────────────────────┐
│  게임 스레드 (싱글스레드)                                       │
│                                                             │
│  Server::create() / Client::create()                        │
│  update()  →  poll_packet()  →  send() / broadcast()        │
└──────────────────────────┬──────────────────────────────────┘
                           │ lock-free queue
┌──────────────────────────┴──────────────────────────────────┐
│  I/O 스레드 (멀티스레드)                                       │
│                                                             │
│  ┌─────────┐  ┌─────────┐  ┌─────────┐                     │
│  │Session 1│  │Session 2│  │Session N│   (서버)             │
│  │ strand  │  │ strand  │  │ strand  │                      │
│  └────┬────┘  └────┬────┘  └────┬────┘                     │
│       └────────────┴────────────┘                           │
│                    │                                        │
│        ┌───────────┴───────────┐                            │
│        │    TCP Socket I/O     │                            │
│        │   read ↔ write chain  │                            │
│        │   heartbeat timer     │                            │
│        └───────────────────────┘                            │
└─────────────────────────────────────────────────────────────┘
```

### 스레딩 모델

- **Server**: `std::thread::hardware_concurrency()` 개수의 I/O 스레드 운용
- **Client**: 단일 I/O 스레드 (소켓 1개이므로 충분)
- `executor_work_guard`로 `io_context::run()` 유지, 소멸자에서 자동 정리
- public API는 소유 객체가 살아 있는 동안 게임 스레드 한 곳에서 호출하는 것을 기본 계약으로 한다.
- `send()`, `broadcast()`, `disconnect()`는 I/O 스레드로 작업을 넘기지만, 객체 소멸과 동시에 다른 스레드에서 호출하는 패턴은 지원하지 않는다.

### 패킷 큐

| 컴포넌트 | 큐 타입 | 용량 | 모델 |
|---------|--------|------|------|
| Server | `boost::lockfree::queue<Packet*>` | 8192 | MPSC (다수 I/O → 게임) |
| Client | `boost::lockfree::spsc_queue<std::vector<uint8_t>*>` | 4096 | SPSC (I/O → 게임) |

## 와이어 프로토콜

고정 8바이트 헤더 + 가변 페이로드 구조:

```
 0      1      2      3      4      5      6      7
┌──────┬──────┬──────┬──────┬──────┬──────┬──────┬──────┬─────────────┐
│  magic (BE) │  ver │ flags│    payload_length (BE)    │  payload…   │
└──────┴──────┴──────┴──────┴──────┴──────┴──────┴──────┴─────────────┘
```

| 필드 | 크기 | 값 |
|------|------|-----|
| magic | 2 bytes | `0x4E4E` (`'NN'`) |
| version | 1 byte | `1` |
| flags | 1 byte | bit 0 = keepalive |
| payload_length | 4 bytes (big-endian) | 0 ~ 2048 |

- **데이터 프레임**: `flags = 0`, `payload_length = 1 ~ 2048`
- **Keepalive 프레임**: `flags = 0x01`, `payload_length = 0`
- 최대 페이로드 크기: **2048 바이트** (`Protocol::MAX_PAYLOAD_SIZE`)

### 하트비트

- 5초 간격으로 keepalive 프레임 전송 (`HEARTBEAT_INTERVAL`)
- 15초간 수신 활동이 없으면 연결 종료 (`HEARTBEAT_TIMEOUT`)
- 게임 레이어에 노출되지 않으며, 전송 계층 내부에서 자동 처리

## 공개 API

### PacketType / Packet

```cpp
enum class PacketType : uint8_t
{
    Connect,      // 새 클라이언트 접속 이벤트
    Disconnect,   // 클라이언트 연결 해제 이벤트
    Data          // 실제 페이로드 데이터
};

struct Packet
{
    PacketType type;
    uint32_t session_id;
    std::vector<uint8_t> data;   // Data 타입일 때만 유효
};
```

`Connect`와 `Disconnect`는 TCP로 전송되지 않는 내부 이벤트다.
실제로 네트워크를 통해 전송되는 것은 `Data` 타입뿐이다.

### Server

```cpp
namespace NightNetwork
{

class Server
{
public:
    // 지정 포트에서 리슨하는 서버 생성. 즉시 accept 루프와 I/O 스레드 시작.
    static std::expected<Server, std::string> create(unsigned short port);

    // 게임 루프에서 매 틱마다 호출 (현재 예약용)
    void update();

    // 수신 큐에서 패킷을 하나 꺼냄. 비었으면 nullopt 반환.
    std::optional<Packet> poll_packet();

    // 수신 큐 overflow로 버려진 패킷 수 조회
    uint64_t dropped_packet_count() const;

    // 특정 세션에 페이로드 전송 (최대 2048 바이트)
    void send(uint32_t session_id, std::span<const uint8_t> data);

    // 모든 세션에 동일 페이로드 전송
    void broadcast(std::span<const uint8_t> data);

    // 특정 세션 강제 종료. Disconnect 이벤트가 poll_packet()으로 전달됨.
    void disconnect(uint32_t session_id);
};

}
```

### Client

```cpp
namespace NightNetwork
{

class Client
{
public:
    // 서버에 동기 연결 후 I/O 스레드 시작
    static std::expected<Client, std::string> create(
        const std::string& host, unsigned short port);

    // 게임 루프에서 매 틱마다 호출 (현재 예약용)
    void update();

    // 수신 큐에서 페이로드 꺼냄 (wire 헤더 제거된 순수 데이터). 비었으면 nullopt.
    std::optional<std::vector<uint8_t>> poll_packet();

    // 수신 큐 overflow로 버려진 패킷 수 조회
    uint64_t dropped_packet_count() const;

    // 서버에 페이로드 전송 (최대 2048 바이트)
    void send(std::span<const uint8_t> data);

    // 연결 상태 반환 (atomic 기반)
    bool is_connected() const;
};

}
```

## 사용 예시

### 에코 서버

```cpp
#include <NightNetwork/Server.h>

#include <chrono>
#include <print>
#include <thread>

int main()
{
    constexpr unsigned short port = 12345;

    auto server = NightNetwork::Server::create(port);
    if (!server)
    {
        std::println(stderr, "{}", server.error());
        return 1;
    }

    std::println("서버 시작 (포트: {})", port);

    constexpr auto tick_rate = std::chrono::milliseconds(33); // ~30 FPS
    auto next_tick = std::chrono::steady_clock::now();

    while (true)
    {
        server->update();

        while (auto pkt = server->poll_packet())
        {
            switch (pkt->type)
            {
            case NightNetwork::PacketType::Connect:
                std::println("[접속] 클라이언트 #{}", pkt->session_id);
                break;

            case NightNetwork::PacketType::Disconnect:
                std::println("[종료] 클라이언트 #{}", pkt->session_id);
                break;

            case NightNetwork::PacketType::Data:
                std::println("[수신] #{}: {}바이트", pkt->session_id, pkt->data.size());
                server->send(pkt->session_id, pkt->data);
                break;
            }
        }

        next_tick += tick_rate;
        std::this_thread::sleep_until(next_tick);
    }
}
```

### 에코 클라이언트

```cpp
#include <NightNetwork/Client.h>

#include <chrono>
#include <iostream>
#include <print>
#include <string>
#include <thread>

int main()
{
    auto client = NightNetwork::Client::create("localhost", 12345);
    if (!client)
    {
        std::println(stderr, "{}", client.error());
        return 1;
    }

    std::println("서버 연결 완료");

    // 별도 스레드에서 사용자 입력을 전송
    std::thread input_thread([&client]()
    {
        std::string line;
        while (std::getline(std::cin, line))
        {
            line += "\n";
            auto bytes = std::vector<uint8_t>(line.begin(), line.end());
            client->send(bytes);
        }
    });
    input_thread.detach();

    constexpr auto tick_rate = std::chrono::milliseconds(33);
    auto next_tick = std::chrono::steady_clock::now();

    while (client->is_connected())
    {
        client->update();

        while (auto data = client->poll_packet())
        {
            std::print("[에코] {}", std::string(data->begin(), data->end()));
        }

        next_tick += tick_rate;
        std::this_thread::sleep_until(next_tick);
    }

    std::println("연결 종료");
    return 0;
}
```

### 브로드캐스트 채팅 서버

```cpp
#include <NightNetwork/Server.h>

#include <chrono>
#include <print>
#include <thread>

int main()
{
    auto server = NightNetwork::Server::create(9000);
    if (!server)
    {
        std::println(stderr, "{}", server.error());
        return 1;
    }

    constexpr auto tick_rate = std::chrono::milliseconds(16);
    auto next_tick = std::chrono::steady_clock::now();

    while (true)
    {
        server->update();

        while (auto pkt = server->poll_packet())
        {
            switch (pkt->type)
            {
            case NightNetwork::PacketType::Connect:
                std::println("#{} 입장", pkt->session_id);
                break;

            case NightNetwork::PacketType::Disconnect:
                std::println("#{} 퇴장", pkt->session_id);
                break;

            case NightNetwork::PacketType::Data:
                // 수신한 데이터를 모든 클라이언트에 브로드캐스트
                server->broadcast(pkt->data);
                break;
            }
        }

        next_tick += tick_rate;
        std::this_thread::sleep_until(next_tick);
    }
}
```

### 특정 세션 강제 종료

```cpp
// 예: 유효하지 않은 메시지를 보낸 클라이언트를 강제 종료
case NightNetwork::PacketType::Data:
    if (pkt->data.size() > 1024)
    {
        std::println("#{}: 과대 페이로드로 강제 종료", pkt->session_id);
        server->disconnect(pkt->session_id);
    }
    break;
```

### CMake에서 링크하기

NightNetwork는 정적 라이브러리로 빌드된다.
상위 프로젝트의 `CMakeLists.txt`에서 다음과 같이 링크한다:

```cmake
# 루트 CMakeLists.txt
find_package(Boost REQUIRED CONFIG)

add_subdirectory(NightNetwork)
add_subdirectory(MyGameServer)
```

```cmake
# MyGameServer/CMakeLists.txt
add_executable(MyGameServer main.cpp)
target_link_libraries(MyGameServer PRIVATE NightNetwork)
```

`NightNetwork` 타겟이 `include/` 디렉터리를 PUBLIC으로 노출하므로,
별도의 `target_include_directories` 설정 없이 `#include <NightNetwork/Server.h>` 형태로 사용할 수 있다.

### vcpkg 의존성

`vcpkg.json`에 다음 의존성이 필요하다:

```json
{
    "dependencies": [
        "asio",
        "boost-lockfree"
    ]
}
```

## API 요약

### Server

| 메서드 | 반환 타입 | 설명 |
|--------|----------|------|
| `create(port)` | `std::expected<Server, std::string>` | 서버 생성 및 리슨 시작 |
| `update()` | `void` | 매 틱 호출 (예약용) |
| `poll_packet()` | `std::optional<Packet>` | 수신 큐에서 패킷 하나 꺼냄 |
| `dropped_packet_count()` | `uint64_t` | 수신 큐 overflow로 버려진 패킷 수 조회 |
| `send(session_id, data)` | `void` | 특정 세션에 전송 |
| `broadcast(data)` | `void` | 전체 세션에 전송 |
| `disconnect(session_id)` | `void` | 특정 세션 강제 종료 |

### Client

| 메서드 | 반환 타입 | 설명 |
|--------|----------|------|
| `create(host, port)` | `std::expected<Client, std::string>` | 서버에 연결 |
| `update()` | `void` | 매 틱 호출 (예약용) |
| `poll_packet()` | `std::optional<std::vector<uint8_t>>` | 수신 큐에서 데이터 꺼냄 |
| `dropped_packet_count()` | `uint64_t` | 수신 큐 overflow로 버려진 패킷 수 조회 |
| `send(data)` | `void` | 서버에 전송 |
| `is_connected()` | `bool` | 연결 상태 확인 |

## 주의사항

- `poll_packet()`은 `nullopt`이 반환될 때까지 반복 호출해야 한다. 한 틱에 여러 패킷이 쌓일 수 있다.
- `dropped_packet_count()`가 증가하면 게임 루프 처리량이 부족하거나 큐 용량이 부족한 상태다.
- `send()` / `broadcast()`에서 `MAX_PAYLOAD_SIZE`(2048 바이트)를 초과하는 데이터는 무시된다.
- `disconnect()` 후 해당 세션의 `Disconnect` 이벤트가 `poll_packet()`으로 전달된다.
- 이미 종료된 세션에 대한 `send()` / `disconnect()`는 안전하게 무시된다.
- `Server`와 `Client`는 복사 불가, 이동만 가능하다.
