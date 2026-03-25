#pragma once

#include "ServerTypes.h"

#include <optional>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace NightServer
{

class RoomSystem
{
public:
    RoomSystem();

    void AddSession(uint32_t session_id);
    void RemoveSession(uint32_t session_id);
    void RemoveSessionFromCurrentRoom(uint32_t session_id);
    void MoveSessionToRoom(uint32_t session_id, uint32_t room_id);
    void SetLoggedIn(uint32_t session_id, std::string_view display_name);

    SessionState* FindSession(uint32_t session_id);
    const SessionState* FindSession(uint32_t session_id) const;

    RoomState* FindRoom(uint32_t room_id);
    const RoomState* FindRoom(uint32_t room_id) const;

    bool IsDisplayNameTaken(std::string_view display_name, uint32_t except_session_id) const;
    std::vector<SessionState> GetRoomMembers(uint32_t room_id, std::optional<uint32_t> exclude_session_id = std::nullopt) const;

    const std::unordered_map<uint32_t, RoomState>& Rooms() const;

private:
    std::unordered_map<uint32_t, SessionState> sessions_;
    std::unordered_map<uint32_t, RoomState> rooms_;
};

} // namespace NightServer
