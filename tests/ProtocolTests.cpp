#include "RoomSystem.h"
#include "TransportDetail.h"

#include <array>
#include <cassert>
#include <cstdint>
#include <vector>

namespace
{

void TestDecodeHeader()
{
    std::array<uint8_t, NightNetwork::Protocol::HEADER_SIZE> header {};
    NightNetwork::Detail::write_header(header.data(), 0, 3);

    auto decoded = NightNetwork::Detail::decode_header(header.data());
    assert(decoded);
    assert(decoded->magic == NightNetwork::Protocol::MAGIC);
    assert(decoded->version == NightNetwork::Protocol::VERSION);
    assert(decoded->flags == 0);
    assert(decoded->payload_size == 3);
    assert(!decoded->is_keepalive());
}

void TestRejectEmptyDataPayload()
{
    std::array<uint8_t, NightNetwork::Protocol::HEADER_SIZE> header {};
    NightNetwork::Detail::write_header(header.data(), 0, 0);

    auto decoded = NightNetwork::Detail::decode_header(header.data());
    assert(!decoded);
}

void TestKeepaliveHeader()
{
    std::array<uint8_t, NightNetwork::Protocol::HEADER_SIZE> header {};
    NightNetwork::Detail::write_header(header.data(), NightNetwork::Protocol::FLAG_KEEPALIVE, 0);

    auto decoded = NightNetwork::Detail::decode_header(header.data());
    assert(decoded);
    assert(decoded->is_keepalive());
    assert(decoded->payload_size == 0);
}

void TestRoomSystemState()
{
    NightServer::RoomSystem rooms;
    rooms.AddSession(10);
    rooms.SetLoggedIn(10, "Alice");
    rooms.MoveSessionToRoom(10, 1001);

    const auto* session = rooms.FindSession(10);
    assert(session);
    assert(session->logged_in);
    assert(session->display_name == "Alice");
    assert(session->room_id == 1001);

    auto members = rooms.GetRoomMembers(1001);
    assert(members.size() == 1);
    assert(members[0].session_id == 10);

    rooms.RemoveSessionFromCurrentRoom(10);
    session = rooms.FindSession(10);
    assert(session);
    assert(session->room_id == NightServer::no_room_id);
    assert(rooms.GetRoomMembers(1001).empty());
}

} // namespace

int main()
{
    TestDecodeHeader();
    TestRejectEmptyDataPayload();
    TestKeepaliveHeader();
    TestRoomSystemState();
    return 0;
}
