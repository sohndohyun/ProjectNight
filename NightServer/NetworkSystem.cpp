#include "NetworkSystem.h"

#include <NightProtocol/messages_generated.h>

#include <flatbuffers/flatbuffers.h>

namespace NightServer
{
namespace
{

std::string ToUtf8(const flatbuffers::String* value)
{
    return value ? std::string(value->c_str()) : std::string {};
}

} // namespace

NetworkSystem::NetworkSystem(NightNetwork::Server server)
    : server_(std::move(server))
{
}

std::vector<ServerEvent> NetworkSystem::Receive()
{
    std::vector<ServerEvent> events;

    server_.update();

    while (auto packet = server_.poll_packet())
    {
        switch (packet->type)
        {
        case NightNetwork::PacketType::Connect:
            events.push_back(SessionConnectedEvent { .session_id = packet->session_id });
            break;

        case NightNetwork::PacketType::Disconnect:
            events.push_back(SessionDisconnectedEvent { .session_id = packet->session_id });
            break;

        case NightNetwork::PacketType::Data:
            DecodeProtocolPacket(packet->session_id, packet->data, events);
            break;
        }
    }

    return events;
}

void NetworkSystem::Flush(const std::vector<OutgoingMessage>& outgoing_messages, const std::vector<uint32_t>& pending_disconnects)
{
    for (const auto& message : outgoing_messages)
        server_.send(message.session_id, message.payload);

    for (uint32_t session_id : pending_disconnects)
        server_.disconnect(session_id);
}

void NetworkSystem::DecodeProtocolPacket(uint32_t session_id, const std::vector<uint8_t>& data, std::vector<ServerEvent>& events) const
{
    flatbuffers::Verifier verifier(data.data(), data.size());
    if (!NightProtocol::VerifyMessageBuffer(verifier))
    {
        events.push_back(InvalidPacketEvent {
            .session_id = session_id,
            .message = "수신 데이터가 Message 스키마와 일치하지 않습니다.",
        });
        return;
    }

    const auto* message = flatbuffers::GetRoot<NightProtocol::Message>(data.data());
    if (!message)
    {
        events.push_back(InvalidPacketEvent {
            .session_id = session_id,
            .message = "프로토콜 메시지를 역직렬화하지 못했습니다.",
        });
        return;
    }

    switch (message->payload_type())
    {
    case NightProtocol::MessagePayload::LoginRequest:
    {
        const auto* request = message->payload_as_LoginRequest();
        if (!request)
            break;

        events.push_back(LoginRequestEvent {
            .session_id = session_id,
            .request_id = message->request_id(),
            .display_name = ToUtf8(request->display_name()),
        });
        break;
    }

    case NightProtocol::MessagePayload::RoomListRequest:
        events.push_back(RoomListRequestEvent {
            .session_id = session_id,
            .request_id = message->request_id(),
        });
        break;

    case NightProtocol::MessagePayload::JoinRoomRequest:
    {
        const auto* request = message->payload_as_JoinRoomRequest();
        if (!request)
            break;

        events.push_back(JoinRoomRequestEvent {
            .session_id = session_id,
            .request_id = message->request_id(),
            .room_id = request->room_id(),
        });
        break;
    }

    case NightProtocol::MessagePayload::ChatSendRequest:
    {
        const auto* request = message->payload_as_ChatSendRequest();
        if (!request)
            break;

        events.push_back(ChatSendRequestEvent {
            .session_id = session_id,
            .request_id = message->request_id(),
            .content = ToUtf8(request->content()),
        });
        break;
    }

    case NightProtocol::MessagePayload::LeaveRoomRequest:
        events.push_back(LeaveRoomRequestEvent {
            .session_id = session_id,
            .request_id = message->request_id(),
        });
        break;

    case NightProtocol::MessagePayload::DisconnectRequest:
        events.push_back(DisconnectRequestEvent {
            .session_id = session_id,
            .request_id = message->request_id(),
        });
        break;

    default:
        events.push_back(InvalidPacketEvent {
            .session_id = session_id,
            .message = "서버가 처리하지 않는 payload 타입입니다.",
        });
        break;
    }
}

} // namespace NightServer
