#pragma once

#include "RoomSystem.h"
#include "ServerTypes.h"

#include <NightProtocol/messages_generated.h>

#include <vector>

namespace NightServer
{

class GameLogicSystem
{
public:
    explicit GameLogicSystem(RoomSystem& room_system);

    void Update(const std::vector<ServerEvent>& events);

    std::vector<OutgoingMessage> TakeOutgoingMessages();
    std::vector<uint32_t> TakePendingDisconnects();

private:
    RoomSystem& room_system_;
    std::vector<OutgoingMessage> outgoing_messages_;
    std::vector<uint32_t> pending_disconnects_;

    void HandleSessionConnected(const SessionConnectedEvent& event);
    void HandleSessionDisconnected(const SessionDisconnectedEvent& event);
    void HandleInvalidPacket(const InvalidPacketEvent& event);
    void HandleLoginRequest(const LoginRequestEvent& request);
    void HandleRoomListRequest(const RoomListRequestEvent& request);
    void HandleJoinRoomRequest(const JoinRoomRequestEvent& request);
    void HandleChatSendRequest(const ChatSendRequestEvent& request);
    void HandleLeaveRoomRequest(const LeaveRoomRequestEvent& request);
    void HandleDisconnectRequest(const DisconnectRequestEvent& request);

    void NotifyRoomLeave(const SessionState& session);

    void QueueMessage(uint32_t session_id, std::vector<uint8_t> payload);
    void QueueLoginResponse(uint32_t session_id, uint32_t request_id, bool success, NightProtocol::ErrorCode error_code, const SessionState* session);
    void QueueRoomListResponse(uint32_t session_id, uint32_t request_id);
    void QueueJoinRoomResponse(uint32_t session_id, uint32_t request_id, bool success, NightProtocol::ErrorCode error_code, const RoomState* room);
    void QueueLeaveRoomResponse(uint32_t session_id, uint32_t request_id, bool success, NightProtocol::ErrorCode error_code);
    void QueueDisconnectResponse(uint32_t session_id, uint32_t request_id, bool success, NightProtocol::ErrorCode error_code);
    void QueueChatBroadcast(uint32_t session_id, uint32_t room_id, const SessionState& sender, const std::string& content);
    void QueueUserJoinedEvent(uint32_t session_id, uint32_t room_id, const SessionState& joined_user);
    void QueueUserLeftEvent(uint32_t session_id, uint32_t room_id, const SessionState& left_user);
    void QueueSystemMessage(uint32_t session_id, uint32_t room_id, const std::string& content);
};

} // namespace NightServer
