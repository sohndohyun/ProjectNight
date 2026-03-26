# schema

FlatBuffers 기반 채팅 프로토콜 스키마 디렉터리.
`NightNetwork`는 바이트 전송만 담당하고, 이 디렉터리는 애플리케이션 레이어 메시지 구조를 정의한다.

## 디렉터리 구조

```
schema/
├── README.md              # 프로토콜 설명 문서
├── messages.fbs           # FlatBuffers 스키마 정의
└── compile.bat            # C++ 코드 생성 스크립트
```

생성된 헤더 출력 경로:

```
NightCommon/NightProtocol/messages_generated.h
```

## 프로토콜 계층

TCP 전송 데이터는 아래 두 계층으로 구성된다.

1. `NightNetwork` 8바이트 전송 헤더
2. FlatBuffers `Message` 페이로드

```
[NightNetwork 8바이트 헤더][FlatBuffers Message payload]
```

전송 헤더와 heartbeat 규칙은 [`../NightNetwork/README.md`](../NightNetwork/README.md)를 따른다.

## 최상위 메시지

```fbs
table Message {
    request_id: uint32;
    payload: MessagePayload;
}
```

| 필드 | 타입 | 설명 |
|------|------|------|
| `request_id` | `uint32` | 요청/응답 상관관계 식별자 |
| `payload` | `MessagePayload` | 실제 메시지 본문 |

## 공통 타입

### `ErrorCode`

- `NONE`: 오류 없음
- `INVALID_NAME`: 잘못된 표시 이름
- `DUPLICATE_NAME`: 중복 표시 이름
- `ROOM_NOT_FOUND`: 채팅방 없음
- `ROOM_FULL`: 방 입장 불가
- `NOT_IN_ROOM`: 현재 방에 없음
- `INVALID_MESSAGE`: 잘못된 메시지
- `ALREADY_IN_ROOM`: 이미 같은 방에 있음
- `INTERNAL_ERROR`: 서버 내부 오류

### `UserInfo`

- `user_id: uint32`
- `display_name: string`

### `RoomInfo`

- `room_id: uint32`
- `room_name: string`
- `user_count: uint32`

## 메시지 종류

### 요청 / 응답

- `LoginRequest` / `LoginResponse`
- `RoomListRequest` / `RoomListResponse`
- `JoinRoomRequest` / `JoinRoomResponse`
- `ChatSendRequest`
- `LeaveRoomRequest` / `LeaveRoomResponse`
- `DisconnectRequest` / `DisconnectResponse`

### 서버 이벤트

- `ChatBroadcast`: 같은 방 사용자 대상 채팅 브로드캐스트
- `UserJoinedEvent`: 사용자 방 입장 알림
- `UserLeftEvent`: 사용자 방 퇴장 알림
- `SystemMessageEvent`: 시스템 메시지 전달

## `ChatBroadcast` 필드

- `room_id: uint32`
- `sender_id: uint32`
- `sender_name: string`
- `content: string`
- `timestamp: ulong`

## MessagePayload union

```fbs
union MessagePayload {
    LoginRequest,
    LoginResponse,
    RoomListRequest,
    RoomListResponse,
    JoinRoomRequest,
    JoinRoomResponse,
    ChatSendRequest,
    ChatBroadcast,
    LeaveRoomRequest,
    LeaveRoomResponse,
    DisconnectRequest,
    DisconnectResponse,
    UserJoinedEvent,
    UserLeftEvent,
    SystemMessageEvent,
}
```

## 스키마 코드 생성

```bat
schema\compile.bat
```

`schema/*.fbs`를 컴파일하여 `NightCommon/NightProtocol/` 아래에 생성 파일을 출력한다.
