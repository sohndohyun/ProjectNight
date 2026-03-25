#pragma once

#include "ServerTypes.h"

#include <NightNetwork/Server.h>

#include <vector>

namespace NightServer
{

class NetworkSystem
{
public:
    explicit NetworkSystem(NightNetwork::Server server);

    std::vector<ServerEvent> Receive();
    void Flush(const std::vector<OutgoingMessage>& outgoing_messages, const std::vector<uint32_t>& pending_disconnects);

private:
    NightNetwork::Server server_;

    void DecodeProtocolPacket(uint32_t session_id, const std::vector<uint8_t>& data, std::vector<ServerEvent>& events) const;
};

} // namespace NightServer
