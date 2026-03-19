cmake_minimum_required(VERSION 3.8)

if (POLICY CMP0141)
    cmake_policy(SET CMP0141 NEW)
    set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
endif()

project("NightServer")

find_package(Boost REQUIRED CONFIG)

add_subdirectory(
    ${CMAKE_CURRENT_SOURCE_DIR}/../NightNetwork
    ${CMAKE_CURRENT_BINARY_DIR}/NightNetwork
)

add_executable(NightServer "NightServer.cpp")

target_link_libraries(NightServer PRIVATE NightNetwork)

target_compile_options(NightServer PRIVATE /utf-8)

if (CMAKE_VERSION VERSION_GREATER 3.12)
    set_property(TARGET NightServer PROPERTY CXX_STANDARD 23)
endif()
