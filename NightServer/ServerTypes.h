#pragma once

#include <cstdint>
#include <string>
#include <unordered_set>
#include <variant>
#include <vector>

namespace NightServer
{

inline constexpr uint32_t no_room_id = 0;

struct SessionState
{
    uint32_t session_id = 0;
    uint32_t user_id = 0;
    std::string display_name;
    uint32_t room_id = no_room_id;
    bool logged_in = false;
};

struct RoomState
{
    uint32_t room_id = 0;
    std::string room_name;
    std::unordered_set<uint32_t> members;
};

struct SessionConnectedEvent
{
    uint32_t session_id = 0;
};

struct SessionDisconnectedEvent
{
    uint32_t session_id = 0;
};

struct InvalidPacketEvent
{
    uint32_t session_id = 0;
    std::string message;
};

struct LoginRequestEvent
{
    uint32_t session_id = 0;
    uint32_t request_id = 0;
    std::string display_name;
};

struct RoomListRequestEvent
{
    uint32_t session_id = 0;
    uint32_t request_id = 0;
};

struct JoinRoomRequestEvent
{
    uint32_t session_id = 0;
    uint32_t request_id = 0;
    uint32_t room_id = 0;
};

struct ChatSendRequestEvent
{
    uint32_t session_id = 0;
    uint32_t request_id = 0;
    std::string content;
};

struct LeaveRoomRequestEvent
{
    uint32_t session_id = 0;
    uint32_t request_id = 0;
};

struct DisconnectRequestEvent
{
    uint32_t session_id = 0;
    uint32_t request_id = 0;
};

using ServerEvent = std::variant<
    SessionConnectedEvent,
    SessionDisconnectedEvent,
    InvalidPacketEvent,
    LoginRequestEvent,
    RoomListRequestEvent,
    JoinRoomRequestEvent,
    ChatSendRequestEvent,
    LeaveRoomRequestEvent,
    DisconnectRequestEvent>;

struct OutgoingMessage
{
    uint32_t session_id = 0;
    std::vector<uint8_t> payload;
};

} // namespace NightServer
