#include "ChatClientFrame.h"

#include <wx/app.h>

namespace
{

class NightClientApp final : public wxApp
{
public:
    bool OnInit() override
    {
        auto* frame = new ChatClientFrame();
        frame->Show(true);
        return true;
    }
};

} // namespace

wxIMPLEMENT_APP(NightClientApp);
