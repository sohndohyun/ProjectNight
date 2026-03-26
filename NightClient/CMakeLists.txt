find_package(wxWidgets CONFIG REQUIRED COMPONENTS base core)

set(NIGHTCLIENT_SOURCES
    NightClient.cpp
    ChatClientFrame.cpp
    ChatClientFrame.h
    ChatProtocolClient.cpp
    ChatProtocolClient.h
)

if(WIN32)
    add_executable(NightClient WIN32 ${NIGHTCLIENT_SOURCES})
else()
    add_executable(NightClient ${NIGHTCLIENT_SOURCES})
endif()

target_link_libraries(NightClient
    PRIVATE
        NightNetwork
        NightCommon
        wxWidgets::wxWidgets
)
