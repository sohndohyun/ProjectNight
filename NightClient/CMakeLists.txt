find_package(wxWidgets CONFIG REQUIRED COMPONENTS base core)

add_executable(NightClient WIN32
    NightClient.cpp
    ChatClientFrame.cpp
    ChatClientFrame.h
    ChatProtocolClient.cpp
    ChatProtocolClient.h
)

target_link_libraries(NightClient
    PRIVATE
        NightNetwork
        NightCommon
        wxWidgets::wxWidgets
)
