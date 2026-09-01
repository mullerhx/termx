#include "VaultUnlockDialog.h"

namespace
{
    // A blank password would silently produce a "vault" anyone could open
    // by just hitting OK — refuse it outright rather than allow a false
    // sense of security.
    constexpr int kMinPasswordLength = 1;
}

VaultUnlockDialog::VaultUnlockDialog(wxWindow* parent, bool vaultExists)
    : wxDialog(parent, wxID_ANY, "termx — Connection Vault", wxDefaultPosition, wxDefaultSize,
               wxDEFAULT_DIALOG_STYLE),
      m_vaultExists(vaultExists)
{
    auto* topSizer = new wxBoxSizer(wxVERTICAL);

    auto* message = new wxStaticText(this, wxID_ANY,
        vaultExists
            ? "Enter your vault password to unlock your saved connections."
            : "Choose a password to encrypt your saved connections with.\n"
              "There is no way to recover this password if you forget it —\n"
              "the connections file cannot be decrypted without it.");
    topSizer->Add(message, 0, wxALL, 12);

    auto* grid = new wxFlexGridSizer(2, wxSize(8, 8));
    grid->AddGrowableCol(1);

    m_password = new wxTextCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize,
                                wxTE_PASSWORD | wxTE_PROCESS_ENTER);
    grid->Add(new wxStaticText(this, wxID_ANY, "Password:"), 0, wxALIGN_CENTRE_VERTICAL);
    grid->Add(m_password, 1, wxEXPAND);

    topSizer->Add(grid, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 12);

    topSizer->Add(CreateSeparatedButtonSizer(wxOK | wxCANCEL), 0, wxEXPAND | wxALL, 12);

    SetSizerAndFit(topSizer);
    m_password->SetFocus();

    Bind(wxEVT_BUTTON, &VaultUnlockDialog::OnOk, this, wxID_OK);
    m_password->Bind(wxEVT_TEXT_ENTER, [this](wxCommandEvent&) {
        wxCommandEvent okEvent(wxEVT_BUTTON, wxID_OK);
        ProcessWindowEvent(okEvent);
    });
}

void VaultUnlockDialog::OnOk(wxCommandEvent& event)
{
    if (static_cast<int>(m_password->GetValue().length()) < kMinPasswordLength)
    {
        wxMessageBox("Please enter a password.", "termx — Connection Vault",
                     wxOK | wxICON_WARNING, this);
        return;
    }

    event.Skip();
}

wxString VaultUnlockDialog::GetPassword() const
{
    return m_password->GetValue();
}
