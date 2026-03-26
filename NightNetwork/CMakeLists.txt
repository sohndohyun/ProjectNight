find_package(asio CONFIG REQUIRED)
find_package(Boost REQUIRED CONFIG COMPONENTS lockfree)

add_library(NightNetwork STATIC
    src/Server.cpp
    src/Client.cpp
    src/Session.cpp
)

target_include_directories(NightNetwork
    PUBLIC include
    PRIVATE src
)

target_link_libraries(NightNetwork
    PRIVATE
        asio::asio
        Boost::lockfree
)

if(WIN32)
    target_compile_definitions(NightNetwork PRIVATE _WIN32_WINNT=0x0A00)
endif()
