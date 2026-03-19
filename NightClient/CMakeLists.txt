add_executable(NightClient "NightClient.cpp")

target_link_libraries(NightClient PRIVATE NightNetwork NightCommon)
