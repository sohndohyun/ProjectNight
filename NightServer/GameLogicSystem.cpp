#include "GameLogicSystem.h"

#include <NightProtocol/ProtocolUtil.h>

#include <chrono>
#include <cstddef>
#include <print>
#include <string>
#include <utility>

namespace NightServer
{
namespace
{

template <typename... T>
struct Overloaded : T...
{
    using T::operator()...;
};

template <typename... T>
Overloaded(T...) -> Overloaded<T...>;

uint64_t CurrentTimestampMs()
{
    return static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count());
}

constexpr std::size_t max_display_name_bytes = 32;
constexpr std::size_t max_chat_message_bytes = 512;
constexpr std::size_t max_protocol_payload_bytes = 2048;

} // namespace

GameLogicSystem::GameLogicSystem(RoomSystem& room_system)
    : room_system_(room_system)
{
}

void GameLogicSystem::Update(const std::vector<ServerEvent>& events)
{
    for (const auto& event : events)
    {
        std::visit(Overloaded {
            [this](const SessionConnectedEvent& connected)
            {
                HandleSessionConnected(connected);
            },
            [this](const SessionDisconnectedEvent& disconnected)
            {
                HandleSessionDisconnected(disconnected);
            },
            [this](const InvalidPacketEvent& invalid)
            {
                HandleInvalidPacket(invalid);
            },
            [this](const LoginRequestEvent& request)
            {
                HandleLoginRequest(request);
            },
            [this](const RoomListRequestEvent& request)
            {
                HandleRoomListRequest(request);
            },
            [this](const JoinRoomRequestEvent& request)
            {
                HandleJoinRoomRequest(request);
            },
            [this](const ChatSendRequestEvent& request)
            {
                HandleChatSendRequest(request);
            },
            [this](const LeaveRoomRequestEvent& request)
            {
                HandleLeaveRoomRequest(request);
            },
            [this](const DisconnectRequestEvent& request)
            {
                HandleDisconnectRequest(request);
            },
        }, event);
    }
}

std::vector<OutgoingMessage> GameLogicSystem::TakeOutgoingMessages()
{
    auto outgoing_messages = std::move(outgoing_messages_);
    outgoing_messages_.clear();
    return outgoing_messages;
}

std::vector<uint32_t> GameLogicSystem::TakePendingDisconnects()
{
    auto pending_disconnects = std::move(pending_disconnects_);
    pending_disconnects_.clear();
    return pending_disconnects;
}

void GameLogicSystem::HandleSessionConnected(const SessionConnectedEvent& event)
{
    room_system_.AddSession(event.session_id);
    std::println("[접속] 클라이언트 #{}", event.session_id);
}

void GameLogicSystem::HandleSessionDisconnected(const SessionDisconnectedEvent& event)
{
    const auto* session = room_system_.FindSession(event.session_id);
    if (session)
        NotifyRoomLeave(*session);

    room_system_.RemoveSession(event.session_id);
    std::println("[종료] 클라이언트 #{}", event.session_id);
}

void GameLogicSystem::HandleInvalidPacket(const InvalidPacketEvent& event)
{
    QueueSystemMessage(event.session_id, 0, event.message);
    std::println("[프로토콜 오류] 클라이언트 #{}: {}", event.session_id, event.message);
}

void GameLogicSystem::HandleLoginRequest(const LoginRequestEvent& request)
{
    const auto* session = room_system_.FindSession(request.session_id);
    if (!session)
        return;

    if (request.display_name.empty())
    {
        QueueLoginResponse(request.session_id, request.request_id, false, NightProtocol::ErrorCode::INVALID_NAME, nullptr);
        return;
    }

    if (request.display_name.size() > max_display_name_bytes)
    {
        QueueLoginResponse(request.session_id, request.request_id, false, NightProtocol::ErrorCode::INVALID_NAME, nullptr);
        return;
    }

    if (room_system_.IsDisplayNameTaken(request.display_name, request.session_id))
    {
        QueueLoginResponse(request.session_id, request.request_id, false, NightProtocol::ErrorCode::DUPLICATE_NAME, nullptr);
        return;
    }

    room_system_.SetLoggedIn(request.session_id, request.display_name);
    session = room_system_.FindSession(request.session_id);
    QueueLoginResponse(request.session_id, request.request_id, true, NightProtocol::ErrorCode::NONE, session);

    std::println("[로그인] 클라이언트 #{} -> {}", request.session_id, session->display_name);
}

void GameLogicSystem::HandleRoomListRequest(const RoomListRequestEvent& request)
{
    QueueRoomListResponse(request.session_id, request.request_id);
}

void GameLogicSystem::HandleJoinRoomRequest(const JoinRoomRequestEvent& request)
{
    auto* session = room_system_.FindSession(request.session_id);
    const auto* room = room_system_.FindRoom(request.room_id);
    if (!session)
        return;

    if (!session->logged_in)
    {
        QueueJoinRoomResponse(request.session_id, request.request_id, false, NightProtocol::ErrorCode::INVALID_NAME, nullptr);
        return;
    }

    if (!room)
    {
        QueueJoinRoomResponse(request.session_id, request.request_id, false, NightProtocol::ErrorCode::ROOM_NOT_FOUND, nullptr);
        return;
    }

    if (session->room_id == request.room_id)
    {
        QueueJoinRoomResponse(request.session_id, request.request_id, false, NightProtocol::ErrorCode::ALREADY_IN_ROOM, room);
        return;
    }

    if (session->room_id != no_room_id)
    {
        NotifyRoomLeave(*session);
        room_system_.RemoveSessionFromCurrentRoom(request.session_id);
        session = room_system_.FindSession(request.session_id);
    }

    auto existing_members = room_system_.GetRoomMembers(request.room_id);
    room_system_.MoveSessionToRoom(request.session_id, request.room_id);

    session = room_system_.FindSession(request.session_id);
    room = room_system_.FindRoom(request.room_id);

    QueueJoinRoomResponse(request.session_id, request.request_id, true, NightProtocol::ErrorCode::NONE, room);
    QueueSystemMessage(request.session_id, room->room_id, room->room_name + " 방에 입장했습니다.");

    for (const auto& other_session : existing_members)
    {
        if (!other_session.logged_in)
            continue;

        QueueUserJoinedEvent(other_session.session_id, room->room_id, *session);
        QueueUserJoinedEvent(request.session_id, room->room_id, other_session);
    }

    std::println("[입장] 클라이언트 #{} -> 방 #{} ({})", request.session_id, room->room_id, room->room_name);
}

void GameLogicSystem::HandleChatSendRequest(const ChatSendRequestEvent& request)
{
    const auto* session = room_system_.FindSession(request.session_id);
    if (!session || !session->logged_in)
        return;

    if (session->room_id == no_room_id)
    {
        QueueSystemMessage(request.session_id, 0, "채팅을 보내기 전에 먼저 방에 입장해야 합니다.");
        return;
    }

    if (request.content.empty())
    {
        QueueSystemMessage(request.session_id, session->room_id, "빈 메시지는 전송할 수 없습니다.");
        return;
    }

    if (request.content.size() > max_chat_message_bytes)
    {
        QueueSystemMessage(request.session_id, session->room_id, "메시지가 너무 깁니다.");
        return;
    }

    auto members = room_system_.GetRoomMembers(session->room_id);
    for (const auto& member : members)
        QueueChatBroadcast(member.session_id, session->room_id, *session, request.content);

    std::println("[채팅] 방 #{} | {}: {}", session->room_id, session->display_name, request.content);
}

void GameLogicSystem::HandleLeaveRoomRequest(const LeaveRoomRequestEvent& request)
{
    const auto* session = room_system_.FindSession(request.session_id);
    if (!session)
        return;

    if (session->room_id == no_room_id)
    {
        QueueLeaveRoomResponse(request.session_id, request.request_id, false, NightProtocol::ErrorCode::NOT_IN_ROOM);
        return;
    }

    const uint32_t room_id = session->room_id;
    NotifyRoomLeave(*session);
    room_system_.RemoveSessionFromCurrentRoom(request.session_id);

    QueueLeaveRoomResponse(request.session_id, request.request_id, true, NightProtocol::ErrorCode::NONE);
    QueueSystemMessage(request.session_id, room_id, "방에서 퇴장했습니다.");
}

void GameLogicSystem::HandleDisconnectRequest(const DisconnectRequestEvent& request)
{
    QueueDisconnectResponse(request.session_id, request.request_id, true, NightProtocol::ErrorCode::NONE);

    const auto* session = room_system_.FindSession(request.session_id);
    if (session)
        NotifyRoomLeave(*session);

    room_system_.RemoveSession(request.session_id);
    pending_disconnects_.push_back(request.session_id);
}

void GameLogicSystem::NotifyRoomLeave(const SessionState& session)
{
    if (session.room_id == no_room_id || !session.logged_in)
        return;

    auto remaining_members = room_system_.GetRoomMembers(session.room_id, session.session_id);
    for (const auto& member : remaining_members)
        QueueUserLeftEvent(member.session_id, session.room_id, session);
}

static flatbuffers::Offset<NightProtocol::UserInfo> BuildUserInfo(flatbuffers::FlatBufferBuilder& builder, const SessionState& session)
{
    return NightProtocol::CreateUserInfoDirect(builder, session.user_id, session.display_name.c_str());
}

static flatbuffers::Offset<NightProtocol::RoomInfo> BuildRoomInfo(flatbuffers::FlatBufferBuilder& builder, const RoomState& room)
{
    return NightProtocol::CreateRoomInfoDirect(
        builder,
        room.room_id,
        room.room_name.c_str(),
        static_cast<uint32_t>(room.members.size()));
}

void GameLogicSystem::QueueMessage(uint32_t session_id, std::vector<uint8_t> payload)
{
    if (payload.empty() || payload.size() > max_protocol_payload_bytes)
        return;

    outgoing_messages_.push_back(OutgoingMessage {
        .session_id = session_id,
        .payload = std::move(payload),
    });
}

void GameLogicSystem::QueueLoginResponse(
    uint32_t session_id,
    uint32_t request_id,
    bool success,
    NightProtocol::ErrorCode error_code,
    const SessionState* session)
{
    QueueMessage(session_id,
                 NightProtocol::BuildMessage(request_id, NightProtocol::MessagePayload::LoginResponse,
                              [&](flatbuffers::FlatBufferBuilder& builder)
                              {
                                  flatbuffers::Offset<NightProtocol::UserInfo> user;
                                  if (success && session)
                                      user = BuildUserInfo(builder, *session);

                                  return NightProtocol::CreateLoginResponse(builder, success, error_code, user);
                              }));
}

void GameLogicSystem::QueueRoomListResponse(uint32_t session_id, uint32_t request_id)
{
    QueueMessage(session_id,
                 NightProtocol::BuildMessage(request_id, NightProtocol::MessagePayload::RoomListResponse,
                              [&](flatbuffers::FlatBufferBuilder& builder)
                              {
                                  std::vector<flatbuffers::Offset<NightProtocol::RoomInfo>> rooms;
                                  rooms.reserve(room_system_.Rooms().size());
                                  for (const auto& [room_id, room] : room_system_.Rooms())
                                      rooms.push_back(BuildRoomInfo(builder, room));

                                  return NightProtocol::CreateRoomListResponseDirect(builder, &rooms);
                              }));
}

void GameLogicSystem::QueueJoinRoomResponse(
    uint32_t session_id,
    uint32_t request_id,
    bool success,
    NightProtocol::ErrorCode error_code,
    const RoomState* room)
{
    QueueMessage(session_id,
                 NightProtocol::BuildMessage(request_id, NightProtocol::MessagePayload::JoinRoomResponse,
                              [&](flatbuffers::FlatBufferBuilder& builder)
                              {
                                  flatbuffers::Offset<NightProtocol::RoomInfo> room_info;
                                  if (success && room)
                                      room_info = BuildRoomInfo(builder, *room);

                                  return NightProtocol::CreateJoinRoomResponse(builder, success, error_code, room_info);
                              }));
}

void GameLogicSystem::QueueLeaveRoomResponse(
    uint32_t session_id,
    uint32_t request_id,
    bool success,
    NightProtocol::ErrorCode error_code)
{
    QueueMessage(session_id,
                 NightProtocol::BuildMessage(request_id, NightProtocol::MessagePayload::LeaveRoomResponse,
                              [&](flatbuffers::FlatBufferBuilder& builder)
                              {
                                  return NightProtocol::CreateLeaveRoomResponse(builder, success, error_code);
                              }));
}

void GameLogicSystem::QueueDisconnectResponse(
    uint32_t session_id,
    uint32_t request_id,
    bool success,
    NightProtocol::ErrorCode error_code)
{
    QueueMessage(session_id,
                 NightProtocol::BuildMessage(request_id, NightProtocol::MessagePayload::DisconnectResponse,
                              [&](flatbuffers::FlatBufferBuilder& builder)
                              {
                                  return NightProtocol::CreateDisconnectResponse(builder, success, error_code);
                              }));
}

void GameLogicSystem::QueueChatBroadcast(
    uint32_t session_id,
    uint32_t room_id,
    const SessionState& sender,
    const std::string& content)
{
    QueueMessage(session_id,
                 NightProtocol::BuildMessage(0, NightProtocol::MessagePayload::ChatBroadcast,
                              [&](flatbuffers::FlatBufferBuilder& builder)
                              {
                                  return NightProtocol::CreateChatBroadcastDirect(
                                      builder,
                                      room_id,
                                      sender.user_id,
                                      sender.display_name.c_str(),
                                      content.c_str(),
                                      CurrentTimestampMs());
                              }));
}

void GameLogicSystem::QueueUserJoinedEvent(uint32_t session_id, uint32_t room_id, const SessionState& joined_user)
{
    QueueMessage(session_id,
                 NightProtocol::BuildMessage(0, NightProtocol::MessagePayload::UserJoinedEvent,
                              [&](flatbuffers::FlatBufferBuilder& builder)
                              {
                                  auto user = BuildUserInfo(builder, joined_user);
                                  return NightProtocol::CreateUserJoinedEvent(builder, room_id, user);
                              }));
}

void GameLogicSystem::QueueUserLeftEvent(uint32_t session_id, uint32_t room_id, const SessionState& left_user)
{
    QueueMessage(session_id,
                 NightProtocol::BuildMessage(0, NightProtocol::MessagePayload::UserLeftEvent,
                              [&](flatbuffers::FlatBufferBuilder& builder)
                              {
                                  auto user = BuildUserInfo(builder, left_user);
                                  return NightProtocol::CreateUserLeftEvent(builder, room_id, user);
                              }));
}

void GameLogicSystem::QueueSystemMessage(uint32_t session_id, uint32_t room_id, const std::string& content)
{
    QueueMessage(session_id,
                 NightProtocol::BuildMessage(0, NightProtocol::MessagePayload::SystemMessageEvent,
                              [&](flatbuffers::FlatBufferBuilder& builder)
                              {
                                  return NightProtocol::CreateSystemMessageEventDirect(builder, room_id, content.c_str());
                              }));
}

} // namespace NightServer
