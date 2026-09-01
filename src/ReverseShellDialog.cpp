#include "ReverseShellDialog.h"

ReverseShellDialog::ReverseShellDialog(wxWindow* parent)
    : wxDialog(parent, wxID_ANY, "New Reverse Shell Listener", wxDefaultPosition, wxDefaultSize,
               wxDEFAULT_DIALOG_STYLE)
{
    auto* grid = new wxFlexGridSizer(2, wxSize(8, 8));
    grid->AddGrowableCol(1);

    m_port = new wxSpinCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize,
                             wxSP_ARROW_KEYS, 1, 65535, 4444);
    grid->Add(new wxStaticText(this, wxID_ANY, "Listen port:"), 0, wxALIGN_CENTRE_VERTICAL);
    grid->Add(m_port, 1, wxEXPAND);

    m_bindAddress = new wxTextCtrl(this, wxID_ANY);
    grid->Add(new wxStaticText(this, wxID_ANY, "Bind address:"), 0, wxALIGN_CENTRE_VERTICAL);
    grid->Add(m_bindAddress, 1, wxEXPAND);

    auto* topSizer = new wxBoxSizer(wxVERTICAL);
    topSizer->Add(grid, 1, wxEXPAND | wxALL, 12);

    auto* note = new wxStaticText(this, wxID_ANY,
        "Opens a tab listening for one inbound connection on this port, for use\n"
        "only against systems you're authorized to test.");
    note->SetForegroundColour(*wxLIGHT_GREY);
    topSizer->Add(note, 0, wxLEFT | wxRIGHT | wxBOTTOM, 12);

    topSizer->Add(CreateSeparatedButtonSizer(wxOK | wxCANCEL), 0, wxEXPAND | wxALL, 12);

    SetSizerAndFit(topSizer);

    Bind(wxEVT_BUTTON, &ReverseShellDialog::OnOk, this, wxID_OK);
}

void ReverseShellDialog::OnOk(wxCommandEvent& event)
{
    event.Skip();
}

wxArrayString ReverseShellDialog::GetArgv() const
{
    wxArrayString argv;
    argv.Add("nc");
    argv.Add("-l");
    argv.Add("-v");
    argv.Add("-n");
    argv.Add("-p");
    argv.Add(wxString::Format("%d", m_port->GetValue()));

    const wxString bindAddress = m_bindAddress->GetValue().Trim();
    if (!bindAddress.IsEmpty())
        argv.Add(bindAddress);

    return argv;
}

wxString ReverseShellDialog::GetTabTitle() const
{
    return wxString::Format("Reverse :%d", m_port->GetValue());
}
