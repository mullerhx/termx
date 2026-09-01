#pragma once

#include <wx/wx.h>
#include <wx/filepicker.h>
#include <wx/spinctrl.h>

#include "ConnectionStore.h"

class ConnectionDialog : public wxDialog
{
public:
    // Pass an existing profile to pre-fill the fields for editing (its own
    // protocol wins over defaultProtocol); pass nullptr to start from
    // defaults for a brand new connection of the given protocol.
    explicit ConnectionDialog(wxWindow* parent, const ConnectionProfile* toEdit = nullptr,
                              ConnectionProtocol defaultProtocol = ConnectionProtocol::Ssh);

    ConnectionProfile GetProfile() const;

    // Ssh/Sftp: {"ssh"|"sftp", ["-p"|"-P", port,] ["-i", identityFile,]
    //  "-o", "StrictHostKeyChecking=...", ["-o", "UserKnownHostsFile=...",]
    //  "[user@]host"}. Ftp: {"ftp", host, [port]}.
    wxArrayString GetArgv() const;

    // The connection's display name if one was given, otherwise
    // "[user@]host" (or just the host, for Ftp) — suitable as a notebook
    // tab title.
    wxString GetTabTitle() const;

private:
    void OnOk(wxCommandEvent& event);
    void OnProtocolChanged(wxCommandEvent& event);
    wxString BuildTarget() const;
    ConnectionProtocol SelectedProtocol() const;
    void UpdateFieldsForProtocol();

    wxChoice* m_protocol = nullptr;
    wxTextCtrl* m_name = nullptr;
    wxTextCtrl* m_host = nullptr;
    wxTextCtrl* m_username = nullptr;
    wxSpinCtrl* m_port = nullptr;
    wxFilePickerCtrl* m_identityFile = nullptr;
    wxChoice* m_hostKeyChecking = nullptr;
    wxFilePickerCtrl* m_knownHostsFile = nullptr;
};
