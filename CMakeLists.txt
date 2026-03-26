cmake_minimum_required(VERSION 3.25)

project("ProjectNight")

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(flatbuffers CONFIG REQUIRED)

if(MSVC)
    add_compile_options(/utf-8)
endif()

add_subdirectory(NightCommon)
add_subdirectory(NightNetwork)
add_subdirectory(NightServer)
add_subdirectory(NightClient)
