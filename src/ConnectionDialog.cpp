#include "ConnectionDialog.h"

namespace
{
    int DefaultPortFor(ConnectionProtocol protocol)
    {
        return protocol == ConnectionProtocol::Ftp ? 21 : 22;
    }

    bool IsKnownDefaultPort(int port)
    {
        return port == 21 || port == 22;
    }
}

ConnectionDialog::ConnectionDialog(wxWindow* parent, const ConnectionProfile* toEdit,
                                   ConnectionProtocol defaultProtocol)
    : wxDialog(parent, wxID_ANY, toEdit ? "Edit Connection" : "New Connection",
               wxDefaultPosition, wxDefaultSize, wxDEFAULT_DIALOG_STYLE)
{
    const ConnectionProtocol protocol = toEdit ? toEdit->protocol : defaultProtocol;

    auto* grid = new wxFlexGridSizer(2, wxSize(8, 8));
    grid->AddGrowableCol(1);

    wxArrayString protocols;
    protocols.Add("SSH");
    protocols.Add("SFTP");
    protocols.Add("FTP");
    m_protocol = new wxChoice(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, protocols);
    m_protocol->SetSelection(static_cast<int>(protocol));
    grid->Add(new wxStaticText(this, wxID_ANY, "Protocol:"), 0, wxALIGN_CENTRE_VERTICAL);
    grid->Add(m_protocol, 1, wxEXPAND);

    m_name = new wxTextCtrl(this, wxID_ANY, toEdit ? toEdit->name : wxString());
    grid->Add(new wxStaticText(this, wxID_ANY, "Name:"), 0, wxALIGN_CENTRE_VERTICAL);
    grid->Add(m_name, 1, wxEXPAND);

    m_host = new wxTextCtrl(this, wxID_ANY, toEdit ? toEdit->host : wxString());
    grid->Add(new wxStaticText(this, wxID_ANY, "Host:"), 0, wxALIGN_CENTRE_VERTICAL);
    grid->Add(m_host, 1, wxEXPAND);

    m_username = new wxTextCtrl(this, wxID_ANY, toEdit ? toEdit->username : wxGetUserId());
    grid->Add(new wxStaticText(this, wxID_ANY, "Username:"), 0, wxALIGN_CENTRE_VERTICAL);
    grid->Add(m_username, 1, wxEXPAND);

    m_port = new wxSpinCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize,
                             wxSP_ARROW_KEYS, 1, 65535, toEdit ? toEdit->port : DefaultPortFor(protocol));
    grid->Add(new wxStaticText(this, wxID_ANY, "Port:"), 0, wxALIGN_CENTRE_VERTICAL);
    grid->Add(m_port, 1, wxEXPAND);

    m_identityFile = new wxFilePickerCtrl(this, wxID_ANY, toEdit ? toEdit->identityFile : wxString(),
                                           "Identity file (optional)", "*",
                                           wxDefaultPosition, wxDefaultSize,
                                           wxFLP_OPEN | wxFLP_FILE_MUST_EXIST | wxFLP_USE_TEXTCTRL);
    grid->Add(new wxStaticText(this, wxID_ANY, "Identity file:"), 0, wxALIGN_CENTRE_VERTICAL);
    grid->Add(m_identityFile, 1, wxEXPAND);

    wxArrayString hostKeyModes;
    hostKeyModes.Add("Ask (default)");
    hostKeyModes.Add("Strict — reject unknown or changed keys");
    hostKeyModes.Add("Accept new keys — still reject changed keys");
    m_hostKeyChecking = new wxChoice(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, hostKeyModes);
    m_hostKeyChecking->SetSelection(toEdit ? static_cast<int>(toEdit->hostKeyChecking) : 0);
    grid->Add(new wxStaticText(this, wxID_ANY, "Host key verification:"), 0,
              wxALIGN_CENTRE_VERTICAL);
    grid->Add(m_hostKeyChecking, 1, wxEXPAND);

    m_knownHostsFile = new wxFilePickerCtrl(this, wxID_ANY, toEdit ? toEdit->knownHostsFile : wxString(),
                                             "Known hosts file (optional override)", "*",
                                             wxDefaultPosition, wxDefaultSize,
                                             wxFLP_OPEN | wxFLP_USE_TEXTCTRL);
    grid->Add(new wxStaticText(this, wxID_ANY, "Known hosts file:"), 0, wxALIGN_CENTRE_VERTICAL);
    grid->Add(m_knownHostsFile, 1, wxEXPAND);

    auto* topSizer = new wxBoxSizer(wxVERTICAL);
    topSizer->Add(grid, 1, wxEXPAND | wxALL, 12);

    auto* note = new wxStaticText(this, wxID_ANY,
        "\"Strict\" and \"Accept new keys\" both refuse to connect if the host's key\n"
        "changes since it was last recorded — the standard warning sign of a\n"
        "man-in-the-middle attack. Plain FTP has no equivalent — it predates the\n"
        "concept — and logs in interactively once connected.");
    note->SetForegroundColour(*wxLIGHT_GREY);
    topSizer->Add(note, 0, wxLEFT | wxRIGHT | wxBOTTOM, 12);

    topSizer->Add(CreateSeparatedButtonSizer(wxOK | wxCANCEL), 0, wxEXPAND | wxALL, 12);

    SetSizerAndFit(topSizer);

    Bind(wxEVT_BUTTON, &ConnectionDialog::OnOk, this, wxID_OK);
    m_protocol->Bind(wxEVT_CHOICE, &ConnectionDialog::OnProtocolChanged, this);

    UpdateFieldsForProtocol();
}

ConnectionProtocol ConnectionDialog::SelectedProtocol() const
{
    switch (m_protocol->GetSelection())
    {
        case 1: return ConnectionProtocol::Sftp;
        case 2: return ConnectionProtocol::Ftp;
        default: return ConnectionProtocol::Ssh;
    }
}

void ConnectionDialog::OnProtocolChanged(wxCommandEvent&)
{
    // Follow the new protocol's conventional port, but only while the port
    // still looks like an untouched default — leave a deliberately custom
    // port alone.
    if (IsKnownDefaultPort(m_port->GetValue()))
        m_port->SetValue(DefaultPortFor(SelectedProtocol()));

    UpdateFieldsForProtocol();
}

void ConnectionDialog::UpdateFieldsForProtocol()
{
    // Plain FTP has no key-based auth or host-key verification concept —
    // it predates both and logs in interactively once connected.
    const bool sshLike = SelectedProtocol() != ConnectionProtocol::Ftp;
    m_identityFile->Enable(sshLike);
    m_hostKeyChecking->Enable(sshLike);
    m_knownHostsFile->Enable(sshLike);
}

void ConnectionDialog::OnOk(wxCommandEvent& event)
{
    if (m_host->GetValue().Trim().IsEmpty())
    {
        wxMessageBox("Please enter a host to connect to.", "New Connection",
                     wxOK | wxICON_WARNING, this);
        return;
    }

    event.Skip();
}

wxString ConnectionDialog::BuildTarget() const
{
    const wxString host = m_host->GetValue().Trim();
    const wxString user = m_username->GetValue().Trim();
    return user.IsEmpty() ? host : (user + "@" + host);
}

ConnectionProfile ConnectionDialog::GetProfile() const
{
    ConnectionProfile profile;
    profile.protocol = SelectedProtocol();
    profile.host = m_host->GetValue().Trim();
    profile.username = m_username->GetValue().Trim();
    profile.port = m_port->GetValue();
    profile.identityFile = m_identityFile->GetPath();

    switch (m_hostKeyChecking->GetSelection())
    {
        case 1: profile.hostKeyChecking = SshHostKeyMode::Strict; break;
        case 2: profile.hostKeyChecking = SshHostKeyMode::AcceptNew; break;
        default: profile.hostKeyChecking = SshHostKeyMode::Ask; break;
    }

    profile.knownHostsFile = m_knownHostsFile->GetPath();

    const wxString target = profile.protocol == ConnectionProtocol::Ftp ? profile.host : BuildTarget();
    profile.name = m_name->GetValue().Trim().IsEmpty() ? target : m_name->GetValue().Trim();

    return profile;
}

wxArrayString ConnectionDialog::GetArgv() const
{
    return GetProfile().BuildArgv();
}

wxString ConnectionDialog::GetTabTitle() const
{
    return GetProfile().name;
}
