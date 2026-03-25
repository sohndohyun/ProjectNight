#pragma once

#include <NightNetwork/Client.h>

#include <cstdint>
#include <expected>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

struct ChatRoomInfo
{
    uint32_t room_id = 0;
    std::string room_name;
    uint32_t user_count = 0;
};

struct ChatLoginSucceededEvent
{
    std::string display_name;
};

struct ChatLoginFailedEvent
{
    std::string error_message;
};

struct ChatRoomListEvent
{
    std::vector<ChatRoomInfo> rooms;
};

struct ChatJoinRoomSucceededEvent
{
    ChatRoomInfo room;
};

struct ChatJoinRoomFailedEvent
{
    std::string error_message;
};

struct ChatMessageReceivedEvent
{
    uint32_t room_id = 0;
    std::string sender_name;
    std::string content;
};

struct ChatUserJoinedEvent
{
    uint32_t room_id = 0;
    std::string display_name;
};

struct ChatUserLeftEvent
{
    uint32_t room_id = 0;
    std::string display_name;
};

struct ChatSystemMessageEvent
{
    uint32_t room_id = 0;
    std::string content;
};

struct ChatProtocolErrorEvent
{
    std::string message;
};

using ChatProtocolEvent = std::variant<
    ChatLoginSucceededEvent,
    ChatLoginFailedEvent,
    ChatRoomListEvent,
    ChatJoinRoomSucceededEvent,
    ChatJoinRoomFailedEvent,
    ChatMessageReceivedEvent,
    ChatUserJoinedEvent,
    ChatUserLeftEvent,
    ChatSystemMessageEvent,
    ChatProtocolErrorEvent>;

class ChatProtocolClient
{
public:
    std::expected<void, std::string> Connect(std::string_view host, unsigned short port);
    void Disconnect();

    bool IsConnected() const;
    void Update();

    void SendLoginRequest(std::string_view display_name);
    void SendRoomListRequest();
    void SendJoinRoomRequest(uint32_t room_id);
    void SendChatRequest(std::string_view text);

    std::vector<ChatProtocolEvent> PollEvents();

    static std::vector<ChatProtocolEvent> CreateMockEvents();

private:
    std::optional<NightNetwork::Client> client_;
    uint32_t next_request_id_ = 1;

    uint32_t NextRequestId();
    void SendBuffer(const uint8_t* data, size_t size);
    std::vector<ChatProtocolEvent> DecodePacket(const std::vector<uint8_t>& data) const;
};
