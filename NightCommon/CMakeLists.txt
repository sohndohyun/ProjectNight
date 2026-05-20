set(NIGHT_SCHEMA_FILE "${CMAKE_SOURCE_DIR}/schema/messages.fbs")
set(NIGHT_PROTOCOL_OUTPUT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/NightProtocol")
set(NIGHT_PROTOCOL_GENERATED_HEADER "${NIGHT_PROTOCOL_OUTPUT_DIR}/messages_generated.h")

if(TARGET flatbuffers::flatc)
    set(NIGHT_FLATC_COMMAND flatbuffers::flatc)
else()
    find_program(NIGHT_FLATC_COMMAND NAMES flatc REQUIRED)
endif()

add_custom_command(
    OUTPUT "${NIGHT_PROTOCOL_GENERATED_HEADER}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${NIGHT_PROTOCOL_OUTPUT_DIR}"
    COMMAND "${NIGHT_FLATC_COMMAND}" --cpp --scoped-enums -o "${NIGHT_PROTOCOL_OUTPUT_DIR}" "${NIGHT_SCHEMA_FILE}"
    DEPENDS "${NIGHT_SCHEMA_FILE}"
    COMMENT "Generating NightProtocol FlatBuffers headers"
    VERBATIM
)

add_custom_target(NightProtocolGenerated
    DEPENDS "${NIGHT_PROTOCOL_GENERATED_HEADER}"
)

add_library(NightCommon INTERFACE)
add_dependencies(NightCommon NightProtocolGenerated)

target_include_directories(NightCommon INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(NightCommon INTERFACE flatbuffers::flatbuffers)
