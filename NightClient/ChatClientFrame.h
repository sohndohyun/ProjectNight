#pragma once

#include "ChatProtocolClient.h"

#include <cstdint>
#include <vector>
#include <wx/frame.h>
#include <wx/string.h>
#include <wx/timer.h>

class wxButton;
class wxCommandEvent;
class wxCloseEvent;
class wxListBox;
class wxListEvent;
class wxListView;
class wxStaticText;
class wxTextCtrl;
class wxTimerEvent;

class ChatClientFrame final : public wxFrame
{
public:
    ChatClientFrame();

private:
    struct RoomEntry
    {
        uint32_t room_id = 0;
        wxString room_name;
        uint32_t user_count = 0;
    };

    wxTextCtrl* host_ctrl_ = nullptr;
    wxTextCtrl* port_ctrl_ = nullptr;
    wxTextCtrl* display_name_ctrl_ = nullptr;
    wxButton* connect_button_ = nullptr;
    wxButton* refresh_rooms_button_ = nullptr;
    wxButton* join_room_button_ = nullptr;
    wxListView* room_list_ = nullptr;
    wxStaticText* current_room_label_ = nullptr;
    wxTextCtrl* chat_log_ctrl_ = nullptr;
    wxTextCtrl* message_input_ctrl_ = nullptr;
    wxButton* send_button_ = nullptr;
    wxListBox* user_list_ = nullptr;
    wxTimer poll_timer_;

    ChatProtocolClient protocol_client_;
    std::vector<RoomEntry> rooms_;
    uint32_t current_room_id_ = 0;

    void BuildLayout();
    void BindEvents();
    void SetConnectionState(bool connected);
    void LoadMockData();
    void AppendLine(const wxString& line);
    void AppendSystemMessage(const wxString& message);
    void AppendChatMessage(const wxString& sender, const wxString& content);
    const RoomEntry* GetSelectedRoom() const;
    void ApplyRoomList(const ChatRoomInfo& room);
    void ApplyRoomList(const std::vector<ChatRoomInfo>& rooms);
    void ApplyJoinedRoom(const ChatRoomInfo& room);
    void AddUser(const wxString& name);
    void RemoveUser(const wxString& name);
    void ProcessProtocolEvent(const ChatProtocolEvent& event);
    void OnConnectClicked(wxCommandEvent& event);
    void OnRefreshRoomsClicked(wxCommandEvent& event);
    void OnJoinRoomClicked(wxCommandEvent& event);
    void OnRoomActivated(wxListEvent& event);
    void OnSendClicked(wxCommandEvent& event);
    void OnPollTimer(wxTimerEvent& event);
    void OnClose(wxCloseEvent& event);
};
