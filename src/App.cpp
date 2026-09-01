#include "App.h"
#include "ConnectionStore.h"
#include "MainFrame.h"
#include "VaultUnlockDialog.h"

#include <wx/image.h>

bool App::OnInit()
{
    if (!wxApp::OnInit())
        return false;

    wxImage::AddHandler(new wxPNGHandler());

    // Ask for the vault password before the main window ever appears — the
    // whole point is that connection details are never shown, or written to
    // disk unencrypted, without it.
    VaultUnlockDialog unlockDialog(nullptr, ConnectionStore::VaultFileExists());
    if (unlockDialog.ShowModal() != wxID_OK)
        return false;

    auto* frame = new MainFrame("termx", unlockDialog.GetPassword());
    frame->Show(true);

    return true;
}
