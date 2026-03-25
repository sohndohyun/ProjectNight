#include "ChatProtocolClient.h"

#include <NightProtocol/messages_generated.h>

#include <flatbuffers/flatbuffers.h>

#include <span>
#include <utility>

namespace
{

std::string ToUtf8(const char* value)
{
    return value ? std::string(value) : std::string {};
}

std::string ToUtf8(const flatbuffers::String* value)
{
    return value ? std::string(value->c_str()) : std::string {};
}

ChatRoomInfo ToChatRoomInfo(const NightProtocol::RoomInfo& room)
{
    return ChatRoomInfo {
        .room_id = room.room_id(),
        .room_name = ToUtf8(room.room_name()),
        .user_count = room.user_count(),
    };
}

std::string ToErrorString(NightProtocol::ErrorCode error_code)
{
    return ToUtf8(NightProtocol::EnumNameErrorCode(error_code));
}

} // namespace

std::expected<void, std::string> ChatProtocolClient::Connect(std::string_view host, unsigned short port)
{
    auto created = NightNetwork::Client::create(std::string(host), port);
    if (!created)
        return std::unexpected(created.error());

    client_.emplace(std::move(created.value()));
    next_request_id_ = 1;
    return {};
}

void ChatProtocolClient::Disconnect()
{
    client_.reset();
}

bool ChatProtocolClient::IsConnected() const
{
    return client_ && client_->is_connected();
}

void ChatProtocolClient::Update()
{
    if (client_)
        client_->update();
}

void ChatProtocolClient::SendLoginRequest(std::string_view display_name)
{
    if (!client_)
        return;

    flatbuffers::FlatBufferBuilder builder;
    auto payload = NightProtocol::CreateLoginRequestDirect(builder, display_name.data());
    auto message = NightProtocol::CreateMessage(
        builder,
        NextRequestId(),
        NightProtocol::MessagePayload::LoginRequest,
        payload.Union());
    NightProtocol::FinishMessageBuffer(builder, message);
    SendBuffer(builder.GetBufferPointer(), builder.GetSize());
}

void ChatProtocolClient::SendRoomListRequest()
{
    if (!client_)
        return;

    flatbuffers::FlatBufferBuilder builder;
    auto payload = NightProtocol::CreateRoomListRequest(builder);
    auto message = NightProtocol::CreateMessage(
        builder,
        NextRequestId(),
        NightProtocol::MessagePayload::RoomListRequest,
        payload.Union());
    NightProtocol::FinishMessageBuffer(builder, message);
    SendBuffer(builder.GetBufferPointer(), builder.GetSize());
}

void ChatProtocolClient::SendJoinRoomRequest(uint32_t room_id)
{
    if (!client_)
        return;

    flatbuffers::FlatBufferBuilder builder;
    auto payload = NightProtocol::CreateJoinRoomRequest(builder, room_id);
    auto message = NightProtocol::CreateMessage(
        builder,
        NextRequestId(),
        NightProtocol::MessagePayload::JoinRoomRequest,
        payload.Union());
    NightProtocol::FinishMessageBuffer(builder, message);
    SendBuffer(builder.GetBufferPointer(), builder.GetSize());
}

void ChatProtocolClient::SendChatRequest(std::string_view text)
{
    if (!client_)
        return;

    flatbuffers::FlatBufferBuilder builder;
    auto payload = NightProtocol::CreateChatSendRequestDirect(builder, text.data());
    auto message = NightProtocol::CreateMessage(
        builder,
        NextRequestId(),
        NightProtocol::MessagePayload::ChatSendRequest,
        payload.Union());
    NightProtocol::FinishMessageBuffer(builder, message);
    SendBuffer(builder.GetBufferPointer(), builder.GetSize());
}

std::vector<ChatProtocolEvent> ChatProtocolClient::PollEvents()
{
    std::vector<ChatProtocolEvent> events;
    if (!client_)
        return events;

    while (auto packet = client_->poll_packet())
    {
        auto decoded = DecodePacket(*packet);
        for (auto& event : decoded)
            events.push_back(std::move(event));
    }

    return events;
}

std::vector<ChatProtocolEvent> ChatProtocolClient::CreateMockEvents()
{
    std::vector<ChatProtocolEvent> events;
    events.push_back(ChatRoomListEvent {
        .rooms = {
            ChatRoomInfo { .room_id = 1001, .room_name = "Lobby", .user_count = 12 },
            ChatRoomInfo { .room_id = 1002, .room_name = "Dev", .user_count = 5 },
            ChatRoomInfo { .room_id = 1003, .room_name = "Chat", .user_count = 8 },
        },
    });
    events.push_back(ChatSystemMessageEvent { .room_id = 1001, .content = "Schema-based chat UI skeleton is ready." });
    return events;
}

uint32_t ChatProtocolClient::NextRequestId()
{
    return next_request_id_++;
}

void ChatProtocolClient::SendBuffer(const uint8_t* data, size_t size)
{
    if (!client_ || data == nullptr || size == 0)
        return;

    client_->send(std::span<const uint8_t>(data, size));
}

std::vector<ChatProtocolEvent> ChatProtocolClient::DecodePacket(const std::vector<uint8_t>& data) const
{
    std::vector<ChatProtocolEvent> events;
    if (data.empty())
        return events;

    flatbuffers::Verifier verifier(data.data(), data.size());
    if (!NightProtocol::VerifyMessageBuffer(verifier))
    {
        events.push_back(ChatProtocolErrorEvent { .message = "Received data does not match the Message schema." });
        return events;
    }

    const auto* message = flatbuffers::GetRoot<NightProtocol::Message>(data.data());
    if (!message)
        return events;

    switch (message->payload_type())
    {
    case NightProtocol::MessagePayload::LoginResponse:
    {
        const auto* response = message->payload_as_LoginResponse();
        if (!response)
            break;

        if (response->success())
        {
            const auto* user = response->user();
            events.push_back(ChatLoginSucceededEvent {
                .display_name = user ? ToUtf8(user->display_name()) : std::string {},
            });
        }
        else
        {
            events.push_back(ChatLoginFailedEvent {
                .error_message = ToErrorString(response->error_code()),
            });
        }
        break;
    }

    case NightProtocol::MessagePayload::RoomListResponse:
    {
        const auto* response = message->payload_as_RoomListResponse();
        if (!response)
            break;

        ChatRoomListEvent event;
        const auto* rooms = response->rooms();
        if (rooms)
        {
            for (const auto* room : *rooms)
            {
                if (room)
                    event.rooms.push_back(ToChatRoomInfo(*room));
            }
        }
        events.push_back(std::move(event));
        break;
    }

    case NightProtocol::MessagePayload::JoinRoomResponse:
    {
        const auto* response = message->payload_as_JoinRoomResponse();
        if (!response)
            break;

        if (response->success() && response->room())
        {
            events.push_back(ChatJoinRoomSucceededEvent {
                .room = ToChatRoomInfo(*response->room()),
            });
        }
        else
        {
            events.push_back(ChatJoinRoomFailedEvent {
                .error_message = ToErrorString(response->error_code()),
            });
        }
        break;
    }

    case NightProtocol::MessagePayload::ChatBroadcast:
    {
        const auto* broadcast = message->payload_as_ChatBroadcast();
        if (!broadcast)
            break;

        events.push_back(ChatMessageReceivedEvent {
            .room_id = broadcast->room_id(),
            .sender_name = ToUtf8(broadcast->sender_name()),
            .content = ToUtf8(broadcast->content()),
        });
        break;
    }

    case NightProtocol::MessagePayload::UserJoinedEvent:
    {
        const auto* event = message->payload_as_UserJoinedEvent();
        if (!event || !event->user())
            break;

        events.push_back(ChatUserJoinedEvent {
            .room_id = event->room_id(),
            .display_name = ToUtf8(event->user()->display_name()),
        });
        break;
    }

    case NightProtocol::MessagePayload::UserLeftEvent:
    {
        const auto* event = message->payload_as_UserLeftEvent();
        if (!event || !event->user())
            break;

        events.push_back(ChatUserLeftEvent {
            .room_id = event->room_id(),
            .display_name = ToUtf8(event->user()->display_name()),
        });
        break;
    }

    case NightProtocol::MessagePayload::SystemMessageEvent:
    {
        const auto* event = message->payload_as_SystemMessageEvent();
        if (!event)
            break;

        events.push_back(ChatSystemMessageEvent {
            .room_id = event->room_id(),
            .content = ToUtf8(event->content()),
        });
        break;
    }

    default:
        break;
    }

    return events;
}
