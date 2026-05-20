add_executable(NightServer
    "NightServer.cpp"
    "NetworkSystem.cpp"
    "RoomSystem.cpp"
    "GameLogicSystem.cpp")

target_link_libraries(NightServer PRIVATE NightNetwork NightCommon)
add_dependencies(NightServer NightProtocolGenerated)
