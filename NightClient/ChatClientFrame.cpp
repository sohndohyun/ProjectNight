#include "ChatClientFrame.h"

#include <wx/listctrl.h>
#include <wx/statline.h>
#include <wx/wx.h>

#include <charconv>
#include <type_traits>

namespace
{

wxString ToWxString(const std::string& value)
{
    return wxString::FromUTF8(value);
}

wxString Trimmed(wxString value)
{
    value.Trim(true);
    value.Trim(false);
    return value;
}

std::optional<unsigned short> ParsePort(const wxString& value)
{
    const auto text = Trimmed(value).ToStdString();
    unsigned int port = 0;
    const auto [ptr, ec] = std::from_chars(text.data(), text.data() + text.size(), port);
    if (ec != std::errc {} || ptr != text.data() + text.size() || port > 65535)
        return std::nullopt;

    return static_cast<unsigned short>(port);
}

} // namespace

ChatClientFrame::ChatClientFrame()
    : wxFrame(nullptr, wxID_ANY, "NightClient", wxDefaultPosition, wxSize(1180, 760))
    , poll_timer_(this)
{
    BuildLayout();
    BindEvents();
    SetConnectionState(false);
    LoadMockData();
    poll_timer_.Start(33);
}

void ChatClientFrame::BuildLayout()
{
    auto* root = new wxBoxSizer(wxVERTICAL);

    auto* connection_box = new wxStaticBoxSizer(wxVERTICAL, this, "Connection");
    auto* connection_grid = new wxFlexGridSizer(2, 4, 10, 10);
    connection_grid->AddGrowableCol(1, 1);
    connection_grid->AddGrowableCol(3, 1);

    connection_grid->Add(new wxStaticText(this, wxID_ANY, "Host"), 0, wxALIGN_CENTER_VERTICAL);
    host_ctrl_ = new wxTextCtrl(this, wxID_ANY, "127.0.0.1");
    connection_grid->Add(host_ctrl_, 1, wxEXPAND);

    connection_grid->Add(new wxStaticText(this, wxID_ANY, "Port"), 0, wxALIGN_CENTER_VERTICAL);
    port_ctrl_ = new wxTextCtrl(this, wxID_ANY, "12345");
    connection_grid->Add(port_ctrl_, 1, wxEXPAND);

    connection_grid->Add(new wxStaticText(this, wxID_ANY, "Display Name"), 0, wxALIGN_CENTER_VERTICAL);
    display_name_ctrl_ = new wxTextCtrl(this, wxID_ANY, "NightUser");
    connection_grid->Add(display_name_ctrl_, 1, wxEXPAND);

    connect_button_ = new wxButton(this, wxID_ANY, "Connect");
    connection_grid->Add(connect_button_, 0, wxEXPAND);

    auto* helper_text = new wxStaticText(
        this,
        wxID_ANY,
        "GUI skeleton based on schema/messages.fbs for login, room list, chat and user events.");

    connection_box->Add(connection_grid, 0, wxEXPAND | wxALL, 8);
    connection_box->Add(helper_text, 0, wxLEFT | wxRIGHT | wxBOTTOM, 8);
    root->Add(connection_box, 0, wxEXPAND | wxALL, 10);

    auto* content = new wxBoxSizer(wxHORIZONTAL);

    auto* rooms_box = new wxStaticBoxSizer(wxVERTICAL, this, "Rooms");
    room_list_ = new wxListView(this, wxID_ANY, wxDefaultPosition, wxSize(300, -1), wxLC_REPORT | wxLC_SINGLE_SEL);
    room_list_->AppendColumn("Room", wxLIST_FORMAT_LEFT, 180);
    room_list_->AppendColumn("Users", wxLIST_FORMAT_RIGHT, 80);

    auto* room_button_row = new wxBoxSizer(wxHORIZONTAL);
    refresh_rooms_button_ = new wxButton(this, wxID_ANY, "Refresh");
    join_room_button_ = new wxButton(this, wxID_ANY, "Join");
    room_button_row->Add(refresh_rooms_button_, 1, wxRIGHT, 6);
    room_button_row->Add(join_room_button_, 1);

    rooms_box->Add(room_list_, 1, wxEXPAND | wxALL, 8);
    rooms_box->Add(room_button_row, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 8);
    content->Add(rooms_box, 0, wxEXPAND | wxRIGHT, 10);

    auto* chat_box = new wxStaticBoxSizer(wxVERTICAL, this, "Chat");
    current_room_label_ = new wxStaticText(this, wxID_ANY, "Current Room: -");
    chat_log_ctrl_ = new wxTextCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize,
        wxTE_MULTILINE | wxTE_READONLY | wxTE_RICH2);

    auto* message_row = new wxBoxSizer(wxHORIZONTAL);
    message_input_ctrl_ = new wxTextCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize,
        wxTE_PROCESS_ENTER);
    send_button_ = new wxButton(this, wxID_ANY, "Send");
    message_row->Add(message_input_ctrl_, 1, wxRIGHT, 6);
    message_row->Add(send_button_, 0);

    chat_box->Add(current_room_label_, 0, wxEXPAND | wxALL, 8);
    chat_box->Add(new wxStaticLine(this), 0, wxEXPAND | wxLEFT | wxRIGHT, 8);
    chat_box->Add(chat_log_ctrl_, 1, wxEXPAND | wxALL, 8);
    chat_box->Add(message_row, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 8);
    content->Add(chat_box, 1, wxEXPAND | wxRIGHT, 10);

    auto* users_box = new wxStaticBoxSizer(wxVERTICAL, this, "Users");
    user_list_ = new wxListBox(this, wxID_ANY);
    users_box->Add(user_list_, 1, wxEXPAND | wxALL, 8);
    content->Add(users_box, 0, wxEXPAND);

    root->Add(content, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 10);

    SetSizer(root);
    CreateStatusBar();
    SetStatusText("Ready");
}

void ChatClientFrame::BindEvents()
{
    connect_button_->Bind(wxEVT_BUTTON, &ChatClientFrame::OnConnectClicked, this);
    refresh_rooms_button_->Bind(wxEVT_BUTTON, &ChatClientFrame::OnRefreshRoomsClicked, this);
    join_room_button_->Bind(wxEVT_BUTTON, &ChatClientFrame::OnJoinRoomClicked, this);
    send_button_->Bind(wxEVT_BUTTON, &ChatClientFrame::OnSendClicked, this);
    message_input_ctrl_->Bind(wxEVT_TEXT_ENTER, &ChatClientFrame::OnSendClicked, this);
    room_list_->Bind(wxEVT_LIST_ITEM_ACTIVATED, &ChatClientFrame::OnRoomActivated, this);
    Bind(wxEVT_TIMER, &ChatClientFrame::OnPollTimer, this, poll_timer_.GetId());
    Bind(wxEVT_CLOSE_WINDOW, &ChatClientFrame::OnClose, this);
}

void ChatClientFrame::SetConnectionState(bool connected)
{
    host_ctrl_->Enable(!connected);
    port_ctrl_->Enable(!connected);
    connect_button_->Enable(!connected);
    refresh_rooms_button_->Enable(connected);
    join_room_button_->Enable(connected);
    message_input_ctrl_->Enable(connected);
    send_button_->Enable(connected);
}

void ChatClientFrame::LoadMockData()
{
    rooms_.clear();
    room_list_->DeleteAllItems();
    user_list_->Clear();
    chat_log_ctrl_->Clear();
    current_room_id_ = 0;
    current_room_label_->SetLabel("Current Room: -");

    for (const auto& event : ChatProtocolClient::CreateMockEvents())
        ProcessProtocolEvent(event);

    SetStatusText("Mock data loaded from schema layout");
}

void ChatClientFrame::AppendLine(const wxString& line)
{
    if (!chat_log_ctrl_->GetValue().empty())
        chat_log_ctrl_->AppendText("\n");

    chat_log_ctrl_->AppendText(line);
}

void ChatClientFrame::AppendSystemMessage(const wxString& message)
{
    AppendLine("[System] " + message);
}

void ChatClientFrame::AppendChatMessage(const wxString& sender, const wxString& content)
{
    AppendLine("[" + sender + "] " + content);
}

const ChatClientFrame::RoomEntry* ChatClientFrame::GetSelectedRoom() const
{
    const long selected = room_list_->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
    if (selected == wxNOT_FOUND || static_cast<size_t>(selected) >= rooms_.size())
        return nullptr;

    return &rooms_[static_cast<size_t>(selected)];
}

void ChatClientFrame::ApplyRoomList(const ChatRoomInfo& room)
{
    const auto row = static_cast<long>(rooms_.size());
    RoomEntry entry {
        .room_id = room.room_id,
        .room_name = ToWxString(room.room_name),
        .user_count = room.user_count,
    };
    rooms_.push_back(entry);

    room_list_->InsertItem(row, entry.room_name);
    room_list_->SetItem(row, 1, wxString::Format("%u", entry.user_count));
}

void ChatClientFrame::ApplyRoomList(const std::vector<ChatRoomInfo>& rooms)
{
    rooms_.clear();
    room_list_->DeleteAllItems();

    for (const auto& room : rooms)
        ApplyRoomList(room);
}

void ChatClientFrame::ApplyJoinedRoom(const ChatRoomInfo& room)
{
    current_room_id_ = room.room_id;
    current_room_label_->SetLabel(
        wxString::Format("Current Room: #%u %s", room.room_id, ToWxString(room.room_name).c_str()));
    SetStatusText(wxString::Format("Joined room #%u", room.room_id));
}

void ChatClientFrame::AddUser(const wxString& name)
{
    if (name.empty())
        return;

    if (user_list_->FindString(name) == wxNOT_FOUND)
        user_list_->Append(name);
}

void ChatClientFrame::RemoveUser(const wxString& name)
{
    const int index = user_list_->FindString(name);
    if (index != wxNOT_FOUND)
        user_list_->Delete(static_cast<unsigned int>(index));
}

void ChatClientFrame::ProcessProtocolEvent(const ChatProtocolEvent& event)
{
    std::visit(
        [this](const auto& value)
        {
            using T = std::decay_t<decltype(value)>;

            if constexpr (std::is_same_v<T, ChatLoginSucceededEvent>)
            {
                const wxString display_name = value.display_name.empty()
                    ? Trimmed(display_name_ctrl_->GetValue())
                    : ToWxString(value.display_name);
                AddUser(display_name);
                SetStatusText("Login completed");
                AppendSystemMessage(display_name + " logged in.");
            }
            else if constexpr (std::is_same_v<T, ChatLoginFailedEvent>)
            {
                AppendSystemMessage("Login failed: " + ToWxString(value.error_message));
            }
            else if constexpr (std::is_same_v<T, ChatRoomListEvent>)
            {
                ApplyRoomList(value.rooms);
                SetStatusText(wxString::Format("Loaded %zu rooms", value.rooms.size()));
            }
            else if constexpr (std::is_same_v<T, ChatJoinRoomSucceededEvent>)
            {
                ApplyJoinedRoom(value.room);
                AppendSystemMessage(wxString::Format("Joined room #%u.", value.room.room_id));
            }
            else if constexpr (std::is_same_v<T, ChatJoinRoomFailedEvent>)
            {
                AppendSystemMessage("Join room failed: " + ToWxString(value.error_message));
            }
            else if constexpr (std::is_same_v<T, ChatMessageReceivedEvent>)
            {
                AppendChatMessage(ToWxString(value.sender_name), ToWxString(value.content));
            }
            else if constexpr (std::is_same_v<T, ChatUserJoinedEvent>)
            {
                const wxString name = ToWxString(value.display_name);
                AddUser(name);
                AppendSystemMessage(name + " joined.");
            }
            else if constexpr (std::is_same_v<T, ChatUserLeftEvent>)
            {
                const wxString name = ToWxString(value.display_name);
                RemoveUser(name);
                AppendSystemMessage(name + " left.");
            }
            else if constexpr (std::is_same_v<T, ChatSystemMessageEvent>)
            {
                AppendSystemMessage(ToWxString(value.content));
            }
            else if constexpr (std::is_same_v<T, ChatProtocolErrorEvent>)
            {
                AppendSystemMessage(ToWxString(value.message));
            }
        },
        event);
}

void ChatClientFrame::OnConnectClicked(wxCommandEvent&)
{
    const wxString host = Trimmed(host_ctrl_->GetValue());
    const auto port = ParsePort(port_ctrl_->GetValue());
    const wxString display_name = Trimmed(display_name_ctrl_->GetValue());

    if (host.empty())
    {
        wxMessageBox("Enter a host.", "NightClient", wxOK | wxICON_WARNING, this);
        return;
    }

    if (!port)
    {
        wxMessageBox("Port must be a number between 0 and 65535.", "NightClient", wxOK | wxICON_WARNING, this);
        return;
    }

    if (display_name.empty())
    {
        wxMessageBox("Enter a display name.", "NightClient", wxOK | wxICON_WARNING, this);
        return;
    }

    auto connected = protocol_client_.Connect(host.ToStdString(), *port);
    if (!connected)
    {
        wxMessageBox(ToWxString(connected.error()), "Connection Failed", wxOK | wxICON_ERROR, this);
        return;
    }

    SetConnectionState(true);
    SetStatusText(wxString::Format("Connected to %s:%u", host.c_str(), *port));
    AppendSystemMessage("TCP connection established. Sending schema-based requests.");

    protocol_client_.SendLoginRequest(display_name.ToStdString());
    protocol_client_.SendRoomListRequest();
}

void ChatClientFrame::OnRefreshRoomsClicked(wxCommandEvent&)
{
    if (!protocol_client_.IsConnected())
        return;

    protocol_client_.SendRoomListRequest();
    AppendSystemMessage("Requested room list.");
}

void ChatClientFrame::OnJoinRoomClicked(wxCommandEvent&)
{
    if (!protocol_client_.IsConnected())
        return;

    const auto* room = GetSelectedRoom();
    if (!room)
    {
        wxMessageBox("Select a room to join.", "NightClient", wxOK | wxICON_INFORMATION, this);
        return;
    }

    AppendSystemMessage(wxString::Format("Sent join request for room #%u.", room->room_id));
    protocol_client_.SendJoinRoomRequest(room->room_id);
}

void ChatClientFrame::OnRoomActivated(wxListEvent&)
{
    wxCommandEvent dummy;
    OnJoinRoomClicked(dummy);
}

void ChatClientFrame::OnSendClicked(wxCommandEvent&)
{
    if (!protocol_client_.IsConnected())
        return;

    if (current_room_id_ == 0)
    {
        wxMessageBox("Join a room first.", "NightClient", wxOK | wxICON_INFORMATION, this);
        return;
    }

    const wxString text = Trimmed(message_input_ctrl_->GetValue());
    if (text.empty())
        return;

    protocol_client_.SendChatRequest(text.ToStdString());
    message_input_ctrl_->Clear();
}

void ChatClientFrame::OnPollTimer(wxTimerEvent&)
{
    if (!protocol_client_.IsConnected())
        return;

    protocol_client_.Update();
    for (const auto& event : protocol_client_.PollEvents())
        ProcessProtocolEvent(event);

    if (!protocol_client_.IsConnected())
    {
        protocol_client_.Disconnect();
        SetConnectionState(false);
        SetStatusText("Disconnected");
        AppendSystemMessage("Server connection closed.");
    }
}

void ChatClientFrame::OnClose(wxCloseEvent& event)
{
    poll_timer_.Stop();
    protocol_client_.Disconnect();
    event.Skip();
}
