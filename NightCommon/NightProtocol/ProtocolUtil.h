#pragma once

#include "messages_generated.h"

#include <flatbuffers/flatbuffers.h>

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace NightProtocol
{

inline std::string ToUtf8(const flatbuffers::String* value)
{
    return value ? std::string(value->c_str(), value->size()) : std::string {};
}

inline std::string ToUtf8(const char* value)
{
    return value ? std::string(value) : std::string {};
}

inline std::string ToErrorString(ErrorCode error_code)
{
    return ToUtf8(EnumNameErrorCode(error_code));
}

inline flatbuffers::Offset<flatbuffers::String> CreateString(
    flatbuffers::FlatBufferBuilder& builder,
    std::string_view value)
{
    return builder.CreateString(value.data(), value.size());
}

inline const Message* VerifiedMessage(const std::vector<uint8_t>& data)
{
    if (data.empty())
        return nullptr;

    flatbuffers::Verifier verifier(data.data(), data.size());
    if (!VerifyMessageBuffer(verifier))
        return nullptr;

    return flatbuffers::GetRoot<Message>(data.data());
}

template <typename BuildPayload>
std::vector<uint8_t> BuildMessage(
    uint32_t request_id,
    MessagePayload payload_type,
    BuildPayload&& build_payload)
{
    flatbuffers::FlatBufferBuilder builder;
    auto payload = build_payload(builder);
    auto message = CreateMessage(builder, request_id, payload_type, payload.Union());
    FinishMessageBuffer(builder, message);

    const auto* begin = builder.GetBufferPointer();
    return std::vector<uint8_t>(begin, begin + builder.GetSize());
}

} // namespace NightProtocol
