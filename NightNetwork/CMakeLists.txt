cmake_minimum_required(VERSION 3.8)

project("NightNetwork")

find_package(Boost REQUIRED CONFIG)

add_library(NightNetwork STATIC
    src/Server.cpp
    src/Client.cpp
    src/Session.cpp
)

target_include_directories(NightNetwork
    PUBLIC include
    PRIVATE src
)

target_link_libraries(NightNetwork PRIVATE Boost::headers)

target_compile_definitions(NightNetwork PRIVATE _WIN32_WINNT=0x0A00)
target_compile_options(NightNetwork PRIVATE /utf-8)

if (CMAKE_VERSION VERSION_GREATER 3.12)
    set_property(TARGET NightNetwork PROPERTY CXX_STANDARD 23)
endif()
