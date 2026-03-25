#include "RoomSystem.h"

namespace NightServer
{

RoomSystem::RoomSystem()
{
    rooms_.emplace(1001, RoomState { .room_id = 1001, .room_name = "Lobby" });
    rooms_.emplace(1002, RoomState { .room_id = 1002, .room_name = "Dev" });
    rooms_.emplace(1003, RoomState { .room_id = 1003, .room_name = "Chat" });
}

void RoomSystem::AddSession(uint32_t session_id)
{
    sessions_.try_emplace(session_id, SessionState {
        .session_id = session_id,
        .user_id = session_id,
    });
}

void RoomSystem::RemoveSession(uint32_t session_id)
{
    RemoveSessionFromCurrentRoom(session_id);
    sessions_.erase(session_id);
}

void RoomSystem::RemoveSessionFromCurrentRoom(uint32_t session_id)
{
    auto* session = FindSession(session_id);
    if (!session || session->room_id == no_room_id)
        return;

    auto* room = FindRoom(session->room_id);
    if (room)
        room->members.erase(session_id);

    session->room_id = no_room_id;
}

void RoomSystem::MoveSessionToRoom(uint32_t session_id, uint32_t room_id)
{
    auto* session = FindSession(session_id);
    auto* room = FindRoom(room_id);
    if (!session || !room)
        return;

    if (session->room_id != no_room_id && session->room_id != room_id)
        RemoveSessionFromCurrentRoom(session_id);

    room->members.insert(session_id);
    session->room_id = room_id;
}

void RoomSystem::SetLoggedIn(uint32_t session_id, std::string_view display_name)
{
    auto* session = FindSession(session_id);
    if (!session)
        return;

    session->logged_in = true;
    session->display_name = std::string(display_name);
}

SessionState* RoomSystem::FindSession(uint32_t session_id)
{
    auto it = sessions_.find(session_id);
    return it != sessions_.end() ? &it->second : nullptr;
}

const SessionState* RoomSystem::FindSession(uint32_t session_id) const
{
    auto it = sessions_.find(session_id);
    return it != sessions_.end() ? &it->second : nullptr;
}

RoomState* RoomSystem::FindRoom(uint32_t room_id)
{
    auto it = rooms_.find(room_id);
    return it != rooms_.end() ? &it->second : nullptr;
}

const RoomState* RoomSystem::FindRoom(uint32_t room_id) const
{
    auto it = rooms_.find(room_id);
    return it != rooms_.end() ? &it->second : nullptr;
}

bool RoomSystem::IsDisplayNameTaken(std::string_view display_name, uint32_t except_session_id) const
{
    for (const auto& [session_id, session] : sessions_)
    {
        if (session_id == except_session_id)
            continue;

        if (session.logged_in && session.display_name == display_name)
            return true;
    }

    return false;
}

std::vector<SessionState> RoomSystem::GetRoomMembers(uint32_t room_id, std::optional<uint32_t> exclude_session_id) const
{
    std::vector<SessionState> members;

    const auto* room = FindRoom(room_id);
    if (!room)
        return members;

    members.reserve(room->members.size());
    for (uint32_t member_session_id : room->members)
    {
        if (exclude_session_id && *exclude_session_id == member_session_id)
            continue;

        const auto* member = FindSession(member_session_id);
        if (member)
            members.push_back(*member);
    }

    return members;
}

const std::unordered_map<uint32_t, RoomState>& RoomSystem::Rooms() const
{
    return rooms_;
}

} // namespace NightServer
