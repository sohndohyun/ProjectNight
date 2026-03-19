add_library(NightCommon INTERFACE)

target_include_directories(NightCommon INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(NightCommon INTERFACE flatbuffers::flatbuffers)
