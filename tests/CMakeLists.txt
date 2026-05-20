add_executable(NightTests
    ProtocolTests.cpp
    "${CMAKE_SOURCE_DIR}/NightServer/RoomSystem.cpp"
)

target_include_directories(NightTests
    PRIVATE
        "${CMAKE_SOURCE_DIR}/NightNetwork/src"
        "${CMAKE_SOURCE_DIR}/NightServer"
)

target_link_libraries(NightTests PRIVATE NightCommon)
add_dependencies(NightTests NightProtocolGenerated)

add_test(NAME NightTests COMMAND NightTests)
